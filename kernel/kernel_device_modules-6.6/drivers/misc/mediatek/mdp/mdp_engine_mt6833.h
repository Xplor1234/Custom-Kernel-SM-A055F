/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDP_ENGINE_H__
#define __MDP_ENGINE_H__

enum CMDQ_ENG_ENUM {
	/* ISP */
	CMDQ_ENG_WPEI = 0,
	CMDQ_ENG_WPEO,      /*  1 */
	CMDQ_ENG_WPEI2,     /*  2 */
	CMDQ_ENG_WPEO2,     /*  3 */
	CMDQ_ENG_ISP_IMGI,  /*  4 */
	CMDQ_ENG_ISP_IMG2O, /*  5 */

	/* MDP */
	CMDQ_ENG_MDP_CAMIN, /*  6 */
	CMDQ_ENG_MDP_CAMIN2,    /*  7 */
	CMDQ_ENG_MDP_RDMA0, /* 8 */
	CMDQ_ENG_MDP_HDR0,  /* 9 */
	CMDQ_ENG_MDP_AAL0,  /* 10 */
	CMDQ_ENG_MDP_RSZ0,  /* 11 */
	CMDQ_ENG_MDP_RSZ1,  /* 12 */
	CMDQ_ENG_MDP_TDSHP0,    /* 13 */
	CMDQ_ENG_MDP_WROT0, /* 14 */
	CMDQ_ENG_MDP_WROT1, /* 15 */

	/* DPE */
	CMDQ_ENG_DPE,       /* 16 */
	CMDQ_ENG_RSC,       /* 17 */
	CMDQ_ENG_GEPF,      /* 18 */
	CMDQ_ENG_EAF,       /* 19 */
	CMDQ_ENG_OWE,       /* 20 */
	CMDQ_ENG_MFB,       /* 21 */
	CMDQ_ENG_FDVT,       /* 22 */

	/* ISP sec */
	CMDQ_ENG_ISP_VIPI,  /* 23 */
	CMDQ_ENG_ISP_LCEI,  /* 24 */
	CMDQ_ENG_ISP_IMG3O, /* 25 */
	CMDQ_ENG_ISP_SMXIO, /* 26 */
	CMDQ_ENG_ISP_DMGI_DEPI, /* 27 */
	CMDQ_ENG_ISP_IMGCI, /* 28 */
	CMDQ_ENG_ISP_UFBCI, /* 29 */
	CMDQ_ENG_ISP_UFBCO, /* 30 */

	CMDQ_ENG_INORDER,   /*31 */

	/* Dummy Engine */
	CMDQ_ENG_DUMMY,     /* 32 */
	CMDQ_ENG_ISP_TIMGO = CMDQ_ENG_DUMMY,
	CMDQ_ENG_ISP_IMGI2 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_ISP_IMG2O2 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_RSZ2 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_WDMA = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_HDR1 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_COLOR1 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_RDMA1 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_COLOR0 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_AAL1 = CMDQ_ENG_DUMMY,
	CMDQ_ENG_MDP_TDSHP1 = CMDQ_ENG_DUMMY,

	CMDQ_ENG_MAX = CMDQ_MAX_ENGINE_COUNT   /* ALWAYS keep at the end */
};

#define MDP_ENG_LARB2 ((1LL << CMDQ_ENG_MDP_RDMA0) | \
		       (1LL << CMDQ_ENG_MDP_WROT0) | \
		       (1LL << CMDQ_ENG_MDP_WROT1))

#define CMDQ_ENG_WPE_GROUP_BITS ((1LL << CMDQ_ENG_WPEI) |  \
				(1LL << CMDQ_ENG_WPEO) |  \
				(1LL << CMDQ_ENG_WPEI2) |  \
				(1LL << CMDQ_ENG_WPEO2))

#define CMDQ_ENG_ISP_GROUP_BITS ((1LL << CMDQ_ENG_ISP_IMGI) |  \
				(1LL << CMDQ_ENG_ISP_IMG2O))

#define CMDQ_ENG_ISP_GROUP_ALL_BITS	(CMDQ_ENG_ISP_GROUP_BITS |	\
				 CMDQ_ENG_WPE_GROUP_BITS)

#define CMDQ_ENG_MTEE_GROUP_BITS	((1LL << CMDQ_ENG_ISP_IMGI) |	\
					 (1LL << CMDQ_ENG_ISP_VIPI) |	\
					 (1LL << CMDQ_ENG_ISP_LCEI) |	\
					 (1LL << CMDQ_ENG_MDP_RDMA0) | \
					 (1LL << CMDQ_ENG_MDP_WROT0) | \
					 (1LL << CMDQ_ENG_MDP_WROT1))

#define CMDQ_ENG_MDP_GROUP_BITS ((1LL << CMDQ_ENG_MDP_CAMIN) |  \
				(1LL << CMDQ_ENG_MDP_CAMIN2) | \
				(1LL << CMDQ_ENG_MDP_RDMA0) |  \
				(1LL << CMDQ_ENG_MDP_RDMA1) |  \
				(1LL << CMDQ_ENG_MDP_RSZ0) |   \
				(1LL << CMDQ_ENG_MDP_RSZ1) |   \
				(1LL << CMDQ_ENG_MDP_TDSHP0) | \
				(1LL << CMDQ_ENG_MDP_TDSHP1) | \
				(1LL << CMDQ_ENG_MDP_COLOR0) | \
				(1LL << CMDQ_ENG_MDP_WROT0) |  \
				(1LL << CMDQ_ENG_MDP_WROT1) |  \
				(1LL << CMDQ_ENG_MDP_AAL0) |   \
				(1LL << CMDQ_ENG_MDP_AAL1) |   \
				(1LL << CMDQ_ENG_MDP_HDR0))

#define CMDQ_ENG_DPE_GROUP_BITS     (1LL << CMDQ_ENG_DPE)
#define CMDQ_ENG_RSC_GROUP_BITS     (1LL << CMDQ_ENG_RSC)
#define CMDQ_ENG_GEPF_GROUP_BITS    (1LL << CMDQ_ENG_GEPF)
#define CMDQ_ENG_EAF_GROUP_BITS     (1LL << CMDQ_ENG_EAF)
#define CMDQ_ENG_OWE_GROUP_BITS     (1LL << CMDQ_ENG_OWE)
#define CMDQ_ENG_MFB_GROUP_BITS     (1LL << CMDQ_ENG_MFB)
#define CMDQ_ENG_FDVT_GROUP_BITS     (1LL << CMDQ_ENG_FDVT)

#define CMDQ_ENG_ISP_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_ISP_GROUP_BITS))
#define CMDQ_ENG_MDP_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_MDP_GROUP_BITS))
#define CMDQ_ENG_DPE_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_DPE_GROUP_BITS))
#define CMDQ_ENG_RSC_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_RSC_GROUP_BITS))
#define CMDQ_ENG_GEPF_GROUP_FLAG(flag)  ((flag) & (CMDQ_ENG_GEPF_GROUP_BITS))
#define CMDQ_ENG_WPE_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_WPE_GROUP_BITS))
#define CMDQ_ENG_EAF_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_EAF_GROUP_BITS))
#define CMDQ_ENG_OWE_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_OWE_GROUP_BITS))
#define CMDQ_ENG_MFB_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_MFB_GROUP_BITS))
#define CMDQ_ENG_FDVT_GROUP_FLAG(flag)  ((flag) & (CMDQ_ENG_FDVT_GROUP_BITS))

#define CMDQ_FOREACH_GROUP(ACTION_struct)\
	ACTION_struct(CMDQ_GROUP_ISP, ISP)  \
	ACTION_struct(CMDQ_GROUP_MDP, MDP)  \
	ACTION_struct(CMDQ_GROUP_DPE, DPE)  \
	ACTION_struct(CMDQ_GROUP_RSC, RSC)  \
	ACTION_struct(CMDQ_GROUP_GEPF, GEPF)    \
	ACTION_struct(CMDQ_GROUP_WPE, WPE)  \
	ACTION_struct(CMDQ_GROUP_EAF, EAF)  \
	ACTION_struct(CMDQ_GROUP_OWE, OWE)  \
	ACTION_struct(CMDQ_GROUP_MFB, MFB)  \
	ACTION_struct(CMDQ_GROUP_FDVT, FDVT)

#define MDP_GENERATE_ENUM(_enum, _string) _enum,

enum CMDQ_GROUP_ENUM {
	CMDQ_FOREACH_GROUP(MDP_GENERATE_ENUM)
	CMDQ_MAX_GROUP_COUNT,   /* ALWAYS keep at the end */
};

enum MDP_ENG_BASE {
	ENGBASE_MMSYS_CONFIG,
	ENGBASE_MDP_RDMA0,
	ENGBASE_MDP_AAL0,
	ENGBASE_MDP_HDR0,
	ENGBASE_MDP_RSZ0,
	ENGBASE_MDP_RSZ1,
	ENGBASE_MDP_WROT0,
	ENGBASE_MDP_WROT1,
	ENGBASE_MDP_TDSHP0,
	ENGBASE_MMSYS_MUTEX,
	ENGBASE_IMGSYS,
	ENGBASE_IMGSYS2_CONFIG,
	ENGBASE_ISP_DIP_A0,
	ENGBASE_ISP_DIP_A1,
	ENGBASE_ISP_DIP_A7,
	ENGBASE_ISP_MSS_B,
	ENGBASE_ISP_MFB_B,
	ENGBASE_COUNT
};

#endif /* __MDP_ENGINE_H__ */
