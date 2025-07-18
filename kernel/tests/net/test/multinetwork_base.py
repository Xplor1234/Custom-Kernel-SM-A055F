#!/usr/bin/python3
#
# Copyright 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Base module for multinetwork tests."""

import errno
import fcntl
import os
import posix
import random
import re
from socket import *  # pylint: disable=wildcard-import
import struct
import time

from scapy import all as scapy

import csocket
import iproute
import net_test


IFF_TUN = 1
IFF_TAP = 2
IFF_NO_PI = 0x1000
TUNSETIFF = 0x400454ca

SO_BINDTODEVICE = 25

# Setsockopt values.
IP_UNICAST_IF = 50
IPV6_MULTICAST_IF = 17
IPV6_UNICAST_IF = 76

# Cmsg values.
IP_TTL = 2
IPV6_2292PKTOPTIONS = 6
IPV6_FLOWINFO = 11
IPV6_HOPLIMIT = 52  # Different from IPV6_UNICAST_HOPS, this is cmsg only.


ACCEPT_RA_MIN_LFT_SYSCTL = "/proc/sys/net/ipv6/conf/default/accept_ra_min_lft"
AUTOCONF_TABLE_SYSCTL = "/proc/sys/net/ipv6/conf/default/accept_ra_rt_table"
IPV4_MARK_REFLECT_SYSCTL = "/proc/sys/net/ipv4/fwmark_reflect"
IPV6_MARK_REFLECT_SYSCTL = "/proc/sys/net/ipv6/fwmark_reflect"
RA_HONOR_PIO_LIFE_SYSCTL = "/proc/sys/net/ipv6/conf/default/ra_honor_pio_life"

HAVE_ACCEPT_RA_MIN_LFT = (os.path.isfile(ACCEPT_RA_MIN_LFT_SYSCTL) or
                          net_test.NonGXI(5, 10) or
                          net_test.KernelAtLeast([(5, 10, 199), (5, 15, 136),
                                                  (6, 1, 57), (6, 6, 0)]))
HAVE_AUTOCONF_TABLE = os.path.isfile(AUTOCONF_TABLE_SYSCTL)
HAVE_RA_HONOR_PIO_LIFE = (os.path.isfile(RA_HONOR_PIO_LIFE_SYSCTL) or
                          net_test.KernelAtLeast([(6, 7, 0)]))


class ConfigurationError(AssertionError):
  pass


class UnexpectedPacketError(AssertionError):
  pass


def MakePktInfo(version, addr, ifindex):
  family = {4: AF_INET, 6: AF_INET6}[version]
  if not addr:
    addr = {4: "0.0.0.0", 6: "::"}[version]
  if addr:
    addr = inet_pton(family, addr)
  if version == 6:
    return csocket.In6Pktinfo((addr, ifindex)).Pack()
  else:
    return csocket.InPktinfo((ifindex, addr, b"\x00" * 4)).Pack()


class MultiNetworkBaseTest(net_test.NetworkTest):
  """Base class for all multinetwork tests.

  This class does not contain any test code, but contains code to set up and
  tear a multi-network environment using multiple tun interfaces. The
  environment is designed to be similar to a real Android device in terms of
  rules and routes, and supports IPv4 and IPv6.

  Tests wishing to use this environment should inherit from this class and
  ensure that any setupClass, tearDownClass, setUp, and tearDown methods they
  implement also call the superclass versions.
  """

  # Must be between 1 and 256, since we put them in MAC addresses and IIDs.
  NETIDS = [100, 150, 200, 250]

  # Stores sysctl values to write back when the test completes.
  saved_sysctls = {}

  # Wether to output setup commands.
  DEBUG = False

  UID_RANGE_START = 2000
  UID_RANGE_END = 9999
  UID_RANGE_SIZE = UID_RANGE_END - UID_RANGE_START + 1

  # Rule priorities.
  PRIORITY_UID = 100
  PRIORITY_OIF = 200
  PRIORITY_FWMARK = 300
  PRIORITY_IIF = 400
  PRIORITY_DEFAULT = 999
  PRIORITY_UNREACHABLE = 1000

  # Actual device routing is more complicated, involving more than one rule
  # per NetId, but here we make do with just one rule that selects the lower
  # 16 bits.
  NETID_FWMASK = 0xffff

  # For convenience.
  IPV4_ADDR = net_test.IPV4_ADDR
  IPV6_ADDR = net_test.IPV6_ADDR
  IPV4_ADDR2 = net_test.IPV4_ADDR2
  IPV6_ADDR2 = net_test.IPV6_ADDR2
  IPV4_PING = net_test.IPV4_PING
  IPV6_PING = net_test.IPV6_PING

  RA_VALIDITY = 600 # seconds

  @classmethod
  def UidRangeForNetid(cls, netid):
    per_netid_range = int(cls.UID_RANGE_SIZE / len(cls.NETIDS))
    idx = cls.NETIDS.index(netid)
    return (
        cls.UID_RANGE_START + per_netid_range * idx,
        cls.UID_RANGE_START + per_netid_range * (idx + 1) - 1
    )

  @classmethod
  def UidForNetid(cls, netid):
    if not netid:
      return 0
    return random.randint(*cls.UidRangeForNetid(netid))

  @classmethod
  def _TableForNetid(cls, netid):
    if cls.AUTOCONF_TABLE_OFFSET and netid in cls.ifindices:
      return cls.ifindices[netid] + (-cls.AUTOCONF_TABLE_OFFSET)
    else:
      return netid

  @staticmethod
  def GetInterfaceName(netid):
    return "nettest%d" % netid

  @staticmethod
  def RouterMacAddress(netid):
    return "02:00:00:00:%02x:00" % netid

  @staticmethod
  def MyMacAddress(netid):
    return "02:00:00:00:%02x:01" % netid

  @staticmethod
  def _RouterAddress(netid, version):
    if version == 6:
      return "fe80::%02x00" % netid
    elif version == 4:
      return "10.0.%d.1" % netid
    else:
      raise ValueError("Don't support IPv%s" % version)

  @classmethod
  def _MyIPv4Address(cls, netid):
    return "10.0.%d.2" % netid

  @classmethod
  def _MyIPv6Address(cls, netid):
    return net_test.GetLinkAddress(cls.GetInterfaceName(netid), False)

  @classmethod
  def MyAddress(cls, version, netid):
    return {4: cls._MyIPv4Address(netid),
            5: cls._MyIPv4Address(netid),
            6: cls._MyIPv6Address(netid)}[version]

  @classmethod
  def MySocketAddress(cls, version, netid):
    addr = cls.MyAddress(version, netid)
    return "::ffff:" + addr if version == 5 else addr

  @classmethod
  def MyLinkLocalAddress(cls, netid):
    return net_test.GetLinkAddress(cls.GetInterfaceName(netid), True)

  @staticmethod
  def OnlinkPrefixLen(version):
    return {4: 24, 6: 64}[version]

  @staticmethod
  def OnlinkPrefix(version, netid):
    return {4: "10.0.%d.0" % netid,
            6: "2001:db8:%02x::" % netid}[version]

  @staticmethod
  def GetRandomDestination(prefix):
    if "." in prefix:
      return prefix + "%d.%d" % (random.randint(0, 255), random.randint(0, 255))
    else:
      return prefix + "%x:%x" % (random.randint(0, 65535),
                                 random.randint(0, 65535))

  def GetProtocolFamily(self, version):
    return {4: AF_INET, 6: AF_INET6}[version]

  @classmethod
  def CreateTunInterface(cls, netid):
    iface = cls.GetInterfaceName(netid)
    try:
      f = open("/dev/net/tun", "r+b", buffering=0)
    except IOError:
      f = open("/dev/tun", "r+b", buffering=0)
    ifr = struct.pack("16sH", iface.encode(), IFF_TAP | IFF_NO_PI)
    ifr += b"\x00" * (40 - len(ifr))
    fcntl.ioctl(f, TUNSETIFF, ifr)
    # Give ourselves a predictable MAC address.
    net_test.SetInterfaceHWAddr(iface, cls.MyMacAddress(netid))
    # Disable DAD so we don't have to wait for it.
    cls.SetSysctl("/proc/sys/net/ipv6/conf/%s/accept_dad" % iface, 0)
    # Set accept_ra to 2, because that's what we use.
    cls.SetSysctl("/proc/sys/net/ipv6/conf/%s/accept_ra" % iface, 2)
    net_test.SetInterfaceUp(iface)
    net_test.SetNonBlocking(f)
    return f

  @classmethod
  def SendRA(cls, netid, retranstimer=None, reachabletime=0, routerlft=RA_VALIDITY,
             piolft=RA_VALIDITY, m=0, o=0, options=()):
    macaddr = cls.RouterMacAddress(netid)
    lladdr = cls._RouterAddress(netid, 6)

    if retranstimer is None:
      # If no retrans timer was specified, pick one that's as long as the
      # router lifetime. This ensures that no spurious ND retransmits
      # will interfere with test expectations.
      retranstimer = routerlft * 1000  # Lifetime is in s, retrans timer in ms.

    # We don't want any routes in the main table. If the kernel doesn't support
    # putting RA routes into per-interface tables, configure routing manually.
    if not HAVE_AUTOCONF_TABLE:
      routerlft = 0

    ra = (scapy.Ether(src=macaddr, dst="33:33:00:00:00:01") /
          scapy.IPv6(src=lladdr, hlim=255) /
          scapy.ICMPv6ND_RA(reachabletime=reachabletime,
                            retranstimer=retranstimer,
                            routerlifetime=routerlft,
                            M=m, O=o) /
          scapy.ICMPv6NDOptSrcLLAddr(lladdr=macaddr) /
          scapy.ICMPv6NDOptPrefixInfo(prefix=cls.OnlinkPrefix(6, netid),
                                      prefixlen=cls.OnlinkPrefixLen(6),
                                      L=1, A=1,
                                      validlifetime=piolft,
                                      preferredlifetime=piolft))
    for option in options:
      ra /= option
    posix.write(cls.tuns[netid].fileno(), bytes(ra))

  @classmethod
  def _RunSetupCommands(cls, netid, is_add):
    for version in [4, 6]:
      # Find out how to configure things.
      iface = cls.GetInterfaceName(netid)
      ifindex = cls.ifindices[netid]
      macaddr = cls.RouterMacAddress(netid)
      router = cls._RouterAddress(netid, version)
      table = cls._TableForNetid(netid)

      # Set up routing rules.
      start, end = cls.UidRangeForNetid(netid)
      cls.iproute.UidRangeRule(version, is_add, start, end, table,
                               cls.PRIORITY_UID)
      cls.iproute.OifRule(version, is_add, iface, table, cls.PRIORITY_OIF)
      cls.iproute.FwmarkRule(version, is_add, netid, cls.NETID_FWMASK, table,
                             cls.PRIORITY_FWMARK)

      # Configure routing and addressing.
      #
      # IPv6 uses autoconf for everything, except if per-device autoconf routing
      # tables are not supported, in which case the default route (only) is
      # configured manually. For IPv4 we have to manually configure addresses,
      # routes, and neighbour cache entries (since we don't reply to ARP or ND).
      #
      # Since deleting addresses also causes routes to be deleted, we need to
      # be careful with ordering or the delete commands will fail with ENOENT.
      #
      # A real Android system will have both IPv4 and IPv6 routes for
      # directly-connected subnets in the per-interface routing tables. Ensure
      # we create those as well.
      do_routing = (version == 4 or cls.AUTOCONF_TABLE_OFFSET is None)
      if is_add:
        if version == 4:
          cls.iproute.AddAddress(cls._MyIPv4Address(netid),
                                 cls.OnlinkPrefixLen(4), ifindex)
          cls.iproute.AddNeighbour(version, router, macaddr, ifindex)
        if do_routing:
          cls.iproute.AddRoute(version, table,
                               cls.OnlinkPrefix(version, netid),
                               cls.OnlinkPrefixLen(version), None, ifindex)
          cls.iproute.AddRoute(version, table, "default", 0, router, ifindex)
      else:
        if do_routing:
          cls.iproute.DelRoute(version, table, "default", 0, router, ifindex)
          cls.iproute.DelRoute(version, table,
                               cls.OnlinkPrefix(version, netid),
                               cls.OnlinkPrefixLen(version), None, ifindex)
        if version == 4:
          cls.iproute.DelNeighbour(version, router, macaddr, ifindex)
          cls.iproute.DelAddress(cls._MyIPv4Address(netid),
                                 cls.OnlinkPrefixLen(4), ifindex)

  @classmethod
  def SetMarkReflectSysctls(cls, value):
    """Makes kernel-generated replies use the mark of the original packet."""
    cls.SetSysctl(IPV4_MARK_REFLECT_SYSCTL, value)
    cls.SetSysctl(IPV6_MARK_REFLECT_SYSCTL, value)

  @classmethod
  def _SetInboundMarking(cls, netid, iface, is_add):
    for version in [4, 6]:
      # Run iptables to set up incoming packet marking.
      add_del = "-A" if is_add else "-D"
      iptables = {4: "iptables", 6: "ip6tables"}[version]
      args = "%s INPUT -t mangle -i %s -j MARK --set-mark %d" % (
          add_del, iface, netid)
      if net_test.RunIptablesCommand(version, args):
        raise ConfigurationError("Setup command failed: %s" % args)

  @classmethod
  def SetInboundMarks(cls, is_add):
    for netid in cls.tuns:
      cls._SetInboundMarking(netid, cls.GetInterfaceName(netid), is_add)

  @classmethod
  def SetDefaultNetwork(cls, netid):
    table = cls._TableForNetid(netid) if netid else None
    for version in [4, 6]:
      is_add = table is not None
      cls.iproute.DefaultRule(version, is_add, table, cls.PRIORITY_DEFAULT)

  @classmethod
  def ClearDefaultNetwork(cls):
    cls.SetDefaultNetwork(None)

  @classmethod
  def GetSysctl(cls, sysctl):
    with open(sysctl, "r") as sysctl_file:
      return sysctl_file.read()

  @classmethod
  def SetSysctl(cls, sysctl, value):
    # Only save each sysctl value the first time we set it. This is so we can
    # set it to arbitrary values multiple times and still write it back
    # correctly at the end.
    if sysctl not in cls.saved_sysctls:
      cls.saved_sysctls[sysctl] = cls.GetSysctl(sysctl)
    with open(sysctl, "w") as sysctl_file:
      sysctl_file.write(str(value) + "\n")

  @classmethod
  def SetIPv6SysctlOnAllIfaces(cls, sysctl, value):
    for netid in cls.tuns:
      iface = cls.GetInterfaceName(netid)
      name = "/proc/sys/net/ipv6/conf/%s/%s" % (iface, sysctl)
      cls.SetSysctl(name, value)

  @classmethod
  def _RestoreSysctls(cls):
    for sysctl, value in cls.saved_sysctls.items():
      try:
        with open(sysctl, "w") as sysctl_file:
          sysctl_file.write(value)
      except IOError:
        pass

  @classmethod
  def _ICMPRatelimitFilename(cls, version):
    return "/proc/sys/net/" + {4: "ipv4/icmp_ratelimit",
                               6: "ipv6/icmp/ratelimit"}[version]

  @classmethod
  def _SetICMPRatelimit(cls, version, limit):
    cls.SetSysctl(cls._ICMPRatelimitFilename(version), limit)

  @classmethod
  def setUpClass(cls):
    # This is per-class setup instead of per-testcase setup because shelling out
    # to ip and iptables is slow, and because routing configuration doesn't
    # change during the test.
    cls.iproute = iproute.IPRoute()
    cls.tuns = {}
    cls.ifindices = {}
    if HAVE_AUTOCONF_TABLE:
      cls.SetSysctl(AUTOCONF_TABLE_SYSCTL, -1000)
      cls.AUTOCONF_TABLE_OFFSET = -1000
    else:
      cls.AUTOCONF_TABLE_OFFSET = None

    # Disable ICMP rate limits. These will be restored by _RestoreSysctls.
    for version in [4, 6]:
      cls._SetICMPRatelimit(version, 0)

    for version in [4, 6]:
      cls.iproute.UnreachableRule(version, True, cls.PRIORITY_UNREACHABLE)

    for netid in cls.NETIDS:
      cls.tuns[netid] = cls.CreateTunInterface(netid)
      iface = cls.GetInterfaceName(netid)
      cls.ifindices[netid] = net_test.GetInterfaceIndex(iface)

      cls.SendRA(netid)
      cls._RunSetupCommands(netid, True)

    # Don't print lots of "device foo entered promiscuous mode" warnings.
    cls.loglevel = cls.GetConsoleLogLevel()
    cls.SetConsoleLogLevel(net_test.KERN_INFO)

    # When running on device, don't send connections through FwmarkServer.
    os.environ["ANDROID_NO_USE_FWMARK_CLIENT"] = "1"

    # Uncomment to look around at interface and rule configuration while
    # running in the background. (Once the test finishes running, all the
    # interfaces and rules are gone.)
    # time.sleep(30)

  @classmethod
  def tearDownClass(cls):
    del os.environ["ANDROID_NO_USE_FWMARK_CLIENT"]

    for version in [4, 6]:
      try:
        cls.iproute.UnreachableRule(version, False, cls.PRIORITY_UNREACHABLE)
      except IOError:
        pass

    for netid in cls.tuns:
      cls._RunSetupCommands(netid, False)
      cls.tuns[netid].close()

    cls.iproute.close()
    cls._RestoreSysctls()
    cls.SetConsoleLogLevel(cls.loglevel)

  def setUp(self):
    self.ClearTunQueues()

  def SetSocketMark(self, s, netid):
    if netid is None:
      netid = 0
    s.setsockopt(SOL_SOCKET, net_test.SO_MARK, netid)

  def GetSocketMark(self, s):
    return s.getsockopt(SOL_SOCKET, net_test.SO_MARK)

  def ClearSocketMark(self, s):
    self.SetSocketMark(s, 0)

  def BindToDevice(self, s, iface):
    if not iface:
      iface = ""
    s.setsockopt(SOL_SOCKET, SO_BINDTODEVICE, iface.encode())

  def SetUnicastInterface(self, s, ifindex):
    # Otherwise, Python thinks it's a 1-byte option.
    ifindex = struct.pack("!I", ifindex)

    # Always set the IPv4 interface, because it will be used even on IPv6
    # sockets if the destination address is a mapped address.
    s.setsockopt(net_test.SOL_IP, IP_UNICAST_IF, ifindex)
    if s.family == AF_INET6:
      s.setsockopt(net_test.SOL_IPV6, IPV6_UNICAST_IF, ifindex)

  def GetRemoteAddress(self, version):
    return {4: self.IPV4_ADDR,
            5: self.IPV4_ADDR,  # see GetRemoteSocketAddress()
            6: self.IPV6_ADDR}[version]

  def GetRemoteSocketAddress(self, version):
    addr = self.GetRemoteAddress(version)
    return "::ffff:" + addr if version == 5 else addr

  def GetOtherRemoteSocketAddress(self, version):
    return {4: self.IPV4_ADDR2,
            5: "::ffff:" + self.IPV4_ADDR2,
            6: self.IPV6_ADDR2}[version]

  def SelectInterface(self, s, netid, mode):
    if mode == "uid":
      os.fchown(s.fileno(), self.UidForNetid(netid), -1)
    elif mode == "mark":
      self.SetSocketMark(s, netid)
    elif mode == "oif":
      iface = self.GetInterfaceName(netid) if netid else ""
      self.BindToDevice(s, iface)
    elif mode == "ucast_oif":
      self.SetUnicastInterface(s, self.ifindices.get(netid, 0))
    else:
      raise ValueError("Unknown interface selection mode %s" % mode)

  def BuildSocket(self, version, constructor, netid, routing_mode):
    if version == 5: version = 6
    s = constructor(self.GetProtocolFamily(version))

    if routing_mode not in [None, "uid"]:
      self.SelectInterface(s, netid, routing_mode)
    elif routing_mode == "uid":
      os.fchown(s.fileno(), self.UidForNetid(netid), -1)

    return s

  def RandomNetid(self, exclude=None):
    """Return a random netid from the list of netids

    Args:
      exclude: a netid or list of netids that should not be chosen
    """
    if exclude is None:
      exclude = []
    elif isinstance(exclude, int):
        exclude = [exclude]
    diff = [netid for netid in self.NETIDS if netid not in exclude]
    return random.choice(diff)

  def SendOnNetid(self, version, s, dstaddr, dstport, netid, payload, cmsgs):
    if netid is not None:
      pktinfo = MakePktInfo(version, None, self.ifindices[netid])
      cmsg_level, cmsg_name = {
          4: (net_test.SOL_IP, csocket.IP_PKTINFO),
          6: (net_test.SOL_IPV6, csocket.IPV6_PKTINFO)}[version]
      cmsgs.append((cmsg_level, cmsg_name, pktinfo))
    csocket.Sendmsg(s, (dstaddr, dstport), payload, cmsgs, csocket.MSG_CONFIRM)

  def ReceiveEtherPacketOn(self, netid, packet):
    posix.write(self.tuns[netid].fileno(), bytes(packet))

  def ReceivePacketOn(self, netid, ip_packet):
    routermac = self.RouterMacAddress(netid)
    mymac = self.MyMacAddress(netid)
    packet = scapy.Ether(src=routermac, dst=mymac) / ip_packet
    self.ReceiveEtherPacketOn(netid, packet)

  def ReadAllPacketsOn(self, netid, include_multicast=False):
    """Return all queued packets on a netid as a list.

    Args:
      netid: The netid from which to read packets
      include_multicast: A boolean, whether to remove multicast packets
        (default=False)
    """
    packets = []
    retries = 0
    max_retries = 1
    while True:
      try:
        packet = posix.read(self.tuns[netid].fileno(), 4096)
        if not packet:
          break
        ether = scapy.Ether(packet)
        # Multicast frames are frames where the first byte of the destination
        # MAC address has 1 in the least-significant bit.
        if include_multicast or not int(ether.dst.split(":")[0], 16) & 0x1:
          packets.append(ether.payload)
      except OSError as e:
        # EAGAIN means there are no more packets waiting.
        if e.errno == errno.EAGAIN:
          # If we didn't see any packets, try again for good luck.
          if not packets and retries < max_retries:
            time.sleep(0.01)
            retries += 1
            continue
          else:
            break
        # Anything else is unexpected.
        else:
          raise e
    return packets

  def InvalidateDstCache(self, version, netid):
    """Invalidates destination cache entries of sockets on the specified table.

    Creates and then deletes a low-priority throw route in the table for the
    given netid, which invalidates the destination cache entries of any sockets
    that refer to routes in that table.

    The fact that this method actually invalidates destination cache entries is
    tested by OutgoingTest#testIPv[46]Remarking, which checks that the kernel
    does not re-route sockets when they are remarked, but does re-route them if
    this method is called.

    Args:
      version: The IP version, 4 or 6.
      netid: The netid to invalidate dst caches on.
    """
    iface = self.GetInterfaceName(netid)
    ifindex = self.ifindices[netid]
    table = self._TableForNetid(netid)
    for action in [iproute.RTM_NEWROUTE, iproute.RTM_DELROUTE]:
      self.iproute._Route(version, iproute.RTPROT_STATIC, action, table,
                          "default", 0, nexthop=None, dev=None, mark=None,
                          uid=None, route_type=iproute.RTN_THROW,
                          priority=100000)

  def ClearTunQueues(self):
    # Keep reading packets on all netids until we get no packets on any of them.
    waiting = None
    while waiting != 0:
      waiting = sum(len(self.ReadAllPacketsOn(netid)) for netid in self.NETIDS)

  def assertPacketMatches(self, expected, actual):
    # The expected packet is just a rough sketch of the packet we expect to
    # receive. For example, it doesn't contain fields we can't predict, such as
    # initial TCP sequence numbers, or that depend on the host implementation
    # and settings, such as TCP options. To check whether the packet matches
    # what we expect, instead of just checking all the known fields one by one,
    # we blank out fields in the actual packet and then compare the whole
    # packets to each other as strings. Because we modify the actual packet,
    # make a copy here.
    actual = actual.copy()

    # Blank out IPv4 fields that we can't predict, like ID and the DF bit.
    actualip = actual.getlayer("IP")
    expectedip = expected.getlayer("IP")
    if actualip and expectedip:
      actualip.id = expectedip.id
      actualip.flags &= 5
      actualip.chksum = None  # Change the header, recalculate the checksum.

    # Blank out the flow label, since new kernels randomize it by default.
    actualipv6 = actual.getlayer("IPv6")
    expectedipv6 = expected.getlayer("IPv6")
    if actualipv6 and expectedipv6:
      actualipv6.fl = expectedipv6.fl

    # Blank out UDP fields that we can't predict (e.g., the source port for
    # kernel-originated packets).
    actualudp = actual.getlayer("UDP")
    expectedudp = expected.getlayer("UDP")
    if actualudp and expectedudp:
      if expectedudp.sport is None:
        actualudp.sport = None
        actualudp.chksum = None
      elif actualudp.chksum == 0xffff and expectedudp.chksum == 0:
        # Scapy does not appear to change 0 to 0xffff as required by RFC 768.
        # It is possible that scapy has been upgraded and this no longer triggers.
        actualudp.chksum = 0

    # Since the TCP code below messes with options, recalculate the length.
    if actualip:
      actualip.len = None
    if actualipv6:
      actualipv6.plen = None

    # Blank out TCP fields that we can't predict.
    actualtcp = actual.getlayer("TCP")
    expectedtcp = expected.getlayer("TCP")
    if actualtcp and expectedtcp:
      actualtcp.dataofs = expectedtcp.dataofs
      actualtcp.options = expectedtcp.options
      actualtcp.window = expectedtcp.window
      if expectedtcp.sport is None:
        actualtcp.sport = None
      if expectedtcp.seq is None:
        actualtcp.seq = None
      if expectedtcp.ack is None:
        actualtcp.ack = None
      actualtcp.chksum = None

    # Serialize the packet so that expected packet fields that are only set when
    # a packet is serialized e.g., the checksum) are filled in.
    expected_real = expected.__class__(bytes(expected))
    actual_real = actual.__class__(bytes(actual))
    # repr() can be expensive. Call it only if the test is going to fail and we
    # want to see the error.
    if expected_real != actual_real:
      self.assertEqual(repr(expected_real), repr(actual_real))

  def PacketMatches(self, expected, actual):
    try:
      self.assertPacketMatches(expected, actual)
      return True
    except AssertionError:
      return False

  def ExpectNoPacketsOn(self, netid, msg):
    packets = self.ReadAllPacketsOn(netid)
    if packets:
      firstpacket = repr(packets[0])
    else:
      firstpacket = ""
    self.assertFalse(packets, msg + ": unexpected packet: " + firstpacket)

  def ExpectPacketOn(self, netid, msg, expected):
    # To avoid confusion due to lots of ICMPv6 ND going on all the time, drop
    # multicast packets unless the packet we expect to see is a multicast
    # packet. For now the only tests that use this are IPv6.
    ipv6 = expected.getlayer("IPv6")
    if ipv6 and ipv6.dst.startswith("ff"):
      include_multicast = True
    else:
      include_multicast = False

    packets = self.ReadAllPacketsOn(netid, include_multicast=include_multicast)
    self.assertTrue(packets, msg + ": received no packets")

    # If we receive a packet that matches what we expected, return it.
    for packet in packets:
      if self.PacketMatches(expected, packet):
        return packet

    # None of the packets matched. Call assertPacketMatches to output a diff
    # between the expected packet and the last packet we received. In theory,
    # we'd output a diff to the packet that's the best match for what we
    # expected, but this is good enough for now.
    try:
      self.assertPacketMatches(expected, packets[-1])
    except Exception as e:
      raise UnexpectedPacketError(
          "%s: diff with last packet:\n%s" % (msg, str(e)))

  def Combinations(self, version):
    """Produces a list of combinations to test."""
    combinations = []

    # Check packets addressed to the IP addresses of all our interfaces...
    for dest_ip_netid in self.tuns:
      ip_if = self.GetInterfaceName(dest_ip_netid)
      myaddr = self.MyAddress(version, dest_ip_netid)
      prefix = {4: "172.22.", 6: "2001:db8:aaaa::"}[version]
      remoteaddr = self.GetRandomDestination(prefix)

      # ... coming in on all our interfaces.
      for netid in self.tuns:
        iif = self.GetInterfaceName(netid)
        combinations.append((netid, iif, ip_if, myaddr, remoteaddr))

    return combinations

  def _FormatMessage(self, iif, ip_if, extra, desc, reply_desc):
    msg = "Receiving %s on %s to %s IP, %s" % (desc, iif, ip_if, extra)
    if reply_desc:
      msg += ": Expecting %s on %s" % (reply_desc, iif)
    else:
      msg += ": Expecting no packets on %s" % iif
    return msg

  def _ReceiveAndExpectResponse(self, netid, packet, reply, msg):
    self.ReceivePacketOn(netid, packet)
    if reply:
      return self.ExpectPacketOn(netid, msg, reply)
    else:
      self.ExpectNoPacketsOn(netid, msg)
      return None


class InboundMarkingTest(MultiNetworkBaseTest):
  """Class that automatically sets up inbound marking."""

  @classmethod
  def setUpClass(cls):
    super(InboundMarkingTest, cls).setUpClass()
    cls.SetInboundMarks(True)

  @classmethod
  def tearDownClass(cls):
    cls.SetInboundMarks(False)
    super(InboundMarkingTest, cls).tearDownClass()
