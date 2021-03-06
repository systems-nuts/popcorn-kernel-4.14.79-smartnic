/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2006 Intel Corp.
 *	Tom Long Nguyen (tom.l.nguyen@intel.com)
 *	Zhang Yanmin (yanmin.zhang@intel.com)
 *
 */

#ifndef _AERDRV_H_
#define _AERDRV_H_

#include <linux/workqueue.h>
#include <linux/pcieport_if.h>
#include <linux/aer.h>
#include <linux/interrupt.h>

#define SYSTEM_ERROR_INTR_ON_MESG_MASK	(PCI_EXP_RTCTL_SECEE|	\
					PCI_EXP_RTCTL_SENFEE|	\
					PCI_EXP_RTCTL_SEFEE)
#define ROOT_PORT_INTR_ON_MESG_MASK	(PCI_ERR_ROOT_CMD_COR_EN|	\
					PCI_ERR_ROOT_CMD_NONFATAL_EN|	\
					PCI_ERR_ROOT_CMD_FATAL_EN)
#define ERR_COR_ID(d)			(d & 0xffff)
#define ERR_UNCOR_ID(d)			(d >> 16)

#define AER_ERROR_SOURCES_MAX		100

#define AER_LOG_TLP_MASKS		(PCI_ERR_UNC_POISON_TLP|	\
					PCI_ERR_UNC_ECRC|		\
					PCI_ERR_UNC_UNSUP|		\
					PCI_ERR_UNC_COMP_ABORT|		\
					PCI_ERR_UNC_UNX_COMP|		\
					PCI_ERR_UNC_MALF_TLP)

#define AER_MAX_MULTI_ERR_DEVICES	5	/* Not likely to have more */

#define AER_MAX_TYPEOF_COR_ERRS		16	/* as per PCI_ERR_COR_STATUS */
#define AER_MAX_TYPEOF_UNCOR_ERRS	26	/* as per PCI_ERR_UNCOR_STATUS*/

struct aer_err_info {
	struct pci_dev *dev[AER_MAX_MULTI_ERR_DEVICES];
	int error_dev_num;

	unsigned int id:16;

	unsigned int severity:2;	/* 0:NONFATAL | 1:FATAL | 2:COR */
	unsigned int __pad1:5;
	unsigned int multi_error_valid:1;

	unsigned int first_error:5;
	unsigned int __pad2:2;
	unsigned int tlp_header_valid:1;

	unsigned int status;		/* COR/UNCOR Error Status */
	unsigned int mask;		/* COR/UNCOR Error Mask */
	struct aer_header_log_regs tlp;	/* TLP Header */
};

struct aer_err_source {
	unsigned int status;
	unsigned int id;
};

struct aer_rpc {
	struct pcie_device *rpd;	/* Root Port device */
	struct work_struct dpc_handler;
	struct aer_err_source e_sources[AER_ERROR_SOURCES_MAX];
	struct aer_err_info e_info;
	unsigned short prod_idx;	/* Error Producer Index */
	unsigned short cons_idx;	/* Error Consumer Index */
	int isr;
	spinlock_t e_lock;		/*
					 * Lock access to Error Status/ID Regs
					 * and error producer/consumer index
					 */
	struct mutex rpc_mutex;		/*
					 * only one thread could do
					 * recovery on the same
					 * root port hierarchy
					 */
};

/* AER stats for the device */
struct aer_stats {

	/*
	 * Fields for all AER capable devices. They indicate the errors
	 * "as seen by this device". Note that this may mean that if an
	 * end point is causing problems, the AER counters may increment
	 * at its link partner (e.g. root port) because the errors will be
	 * "seen" by the link partner and not the the problematic end point
	 * itself (which may report all counters as 0 as it never saw any
	 * problems).
	 */
	/* Counters for different type of correctable errors */
	u64 dev_cor_errs[AER_MAX_TYPEOF_COR_ERRS];
	/* Counters for different type of fatal uncorrectable errors */
	u64 dev_fatal_errs[AER_MAX_TYPEOF_UNCOR_ERRS];
	/* Counters for different type of nonfatal uncorrectable errors */
	u64 dev_nonfatal_errs[AER_MAX_TYPEOF_UNCOR_ERRS];
	/* Total number of ERR_COR sent by this device */
	u64 dev_total_cor_errs;
	/* Total number of ERR_FATAL sent by this device */
	u64 dev_total_fatal_errs;
	/* Total number of ERR_NONFATAL sent by this device */
	u64 dev_total_nonfatal_errs;

	/*
	 * Fields for Root ports & root complex event collectors only, these
	 * indicate the total number of ERR_COR, ERR_FATAL, and ERR_NONFATAL
	 * messages received by the root port / event collector, INCLUDING the
	 * ones that are generated internally (by the rootport itself)
	 */
	u64 rootport_total_cor_errs;
	u64 rootport_total_fatal_errs;
	u64 rootport_total_nonfatal_errs;
};

struct aer_broadcast_data {
	enum pci_channel_state state;
	enum pci_ers_result result;
};

static inline pci_ers_result_t merge_result(enum pci_ers_result orig,
		enum pci_ers_result new)
{
	if (new == PCI_ERS_RESULT_NO_AER_DRIVER)
		return PCI_ERS_RESULT_NO_AER_DRIVER;

	if (new == PCI_ERS_RESULT_NONE)
		return orig;

	switch (orig) {
	case PCI_ERS_RESULT_CAN_RECOVER:
	case PCI_ERS_RESULT_RECOVERED:
		orig = new;
		break;
	case PCI_ERS_RESULT_DISCONNECT:
		if (new == PCI_ERS_RESULT_NEED_RESET)
			orig = PCI_ERS_RESULT_NEED_RESET;
		break;
	default:
		break;
	}

	return orig;
}

extern struct bus_type pcie_port_bus_type;
void aer_isr(struct work_struct *work);
void aer_print_error(struct pci_dev *dev, struct aer_err_info *info);
void aer_print_port_info(struct pci_dev *dev, struct aer_err_info *info);
irqreturn_t aer_irq(int irq, void *context);
void pci_rootport_aer_stats_incr(struct pci_dev *pdev,
				 struct aer_err_source *e_src);

#ifdef CONFIG_ACPI_APEI
int pcie_aer_get_firmware_first(struct pci_dev *pci_dev);
#else
static inline int pcie_aer_get_firmware_first(struct pci_dev *pci_dev)
{
	if (pci_dev->__aer_firmware_first_valid)
		return pci_dev->__aer_firmware_first;
	return 0;
}
#endif
#endif /* _AERDRV_H_ */
