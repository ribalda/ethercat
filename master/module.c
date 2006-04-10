/******************************************************************************
 *
 *  m o d u l e . c
 *
 *  EtherCAT-Master-Treiber
 *
 *  Autoren: Wilhelm Hagemeister, Florian Pose
 *
 *  $Id$
 *
 *  (C) Copyright IgH 2005
 *  Ingenieurgemeinschaft IgH
 *  Heinz-Bäcker Str. 34
 *  D-45356 Essen
 *  Tel.: +49 201/61 99 31
 *  Fax.: +49 201/61 98 36
 *  E-mail: sp@igh-essen.com
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "master.h"
#include "device.h"

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

/*****************************************************************************/

#define EC_LIT(X) #X
#define EC_STR(X) EC_LIT(X)

#define COMPILE_INFO "Revision " EC_STR(SVNREV) \
                     ", compiled by " EC_STR(USER) \
                     " at " __DATE__ " " __TIME__

/*****************************************************************************/

static int ec_master_count = 1;
static struct list_head ec_masters;

/*****************************************************************************/

MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(COMPILE_INFO);

module_param(ec_master_count, int, 1);
MODULE_PARM_DESC(ec_master_count, "Number of EtherCAT master to initialize.");

/*****************************************************************************/

/**
   Init-Funktion des EtherCAT-Master-Treibermodules

   Initialisiert soviele Master, wie im Parameter ec_master_count
   angegeben wurde (Default ist 1).

   \return 0 wenn alles ok, < 0 bei ungültiger Anzahl Master
           oder zu wenig Speicher.
*/

int __init ec_init_module(void)
{
    unsigned int i;
    ec_master_t *master, *next;

    EC_INFO("Master driver, %s\n", COMPILE_INFO);

    if (ec_master_count < 1) {
        EC_ERR("Error - Invalid ec_master_count: %i\n", ec_master_count);
        goto out_return;
    }

    EC_INFO("Initializing %i EtherCAT master(s)...\n", ec_master_count);

    INIT_LIST_HEAD(&ec_masters);

    for (i = 0; i < ec_master_count; i++) {
        if (!(master =
              (ec_master_t *) kmalloc(sizeof(ec_master_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate memory for EtherCAT master %i.\n", i);
            goto out_free;
        }

        if (ec_master_init(master, i)) // kobject_put is done inside...
            goto out_free;

        if (kobject_add(&master->kobj)) {
            EC_ERR("Failed to add kobj.\n");
            kobject_put(&master->kobj); // free master
            goto out_free;
        }

        list_add_tail(&master->list, &ec_masters);
    }

    EC_INFO("Master driver initialized.\n");
    return 0;

 out_free:
    list_for_each_entry_safe(master, next, &ec_masters, list) {
        list_del(&master->list);
        kobject_del(&master->kobj);
        kobject_put(&master->kobj);
    }
 out_return:
    return -1;
}

/*****************************************************************************/

/**
   Cleanup-Funktion des EtherCAT-Master-Treibermoduls

   Entfernt alle Master-Instanzen.
*/

void __exit ec_cleanup_module(void)
{
    ec_master_t *master, *next;

    EC_INFO("Cleaning up master driver...\n");

    list_for_each_entry_safe(master, next, &ec_masters, list) {
        list_del(&master->list);
        kobject_del(&master->kobj);
        kobject_put(&master->kobj); // free master
    }

    EC_INFO("Master driver cleaned up.\n");
}

/*****************************************************************************/

/**
   Gets a handle to a certain master.
   \returns pointer to master
*/

ec_master_t *ec_find_master(unsigned int master_index)
{
    ec_master_t *master;

    list_for_each_entry(master, &ec_masters, list) {
        if (master->index == master_index) return master;
    }

    EC_ERR("Master %i does not exist!\n", master_index);
    return NULL;
}

/******************************************************************************
 *
 * Treiberschnittstelle
 *
 *****************************************************************************/

/**
   Registeriert das EtherCAT-Geraet fuer einen EtherCAT-Master.

   \return 0 wenn alles ok, oder < 0 wenn bereits ein Gerät registriert
           oder das Geraet nicht geöffnet werden konnte.
*/

ec_device_t *ecdev_register(unsigned int master_index,
                            /**< Index des EtherCAT-Masters */
                            struct net_device *net_dev,
                            /**< net_device des EtherCAT-Gerätes */
                            ec_isr_t isr,
                            /**< Interrupt-Service-Routine */
                            struct module *module
                            /**< Zeiger auf das Modul */
                            )
{
    ec_master_t *master;

    if (!net_dev) {
        EC_WARN("Device is NULL!\n");
        return NULL;
    }

    if (!(master = ec_find_master(master_index))) return NULL;

    // critical section start
    if (master->device) {
        EC_ERR("Master %i already has a device!\n", master_index);
        return NULL;
    }

    if (!(master->device =
          (ec_device_t *) kmalloc(sizeof(ec_device_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate device!\n");
        return NULL;
    }
    // critical section end

    if (ec_device_init(master->device, master, net_dev, isr, module)) {
        kfree(master->device);
        master->device = NULL;
        return NULL;
    }

    return master->device;
}

/*****************************************************************************/

/**
   Hebt die Registrierung eines EtherCAT-Gerätes auf.
*/

void ecdev_unregister(unsigned int master_index,
                      /**< Index des EtherCAT-Masters */
                      ec_device_t *device
                      /**< EtherCAT-Geraet */
                      )
{
    ec_master_t *master;

    if (master_index >= ec_master_count) {
        EC_WARN("Master %i does not exist!\n", master_index);
        return;
    }

    if (!(master = ec_find_master(master_index))) return;

    if (!master->device || master->device != device) {
        EC_WARN("Unable to unregister device!\n");
        return;
    }

    ec_device_clear(master->device);
    kfree(master->device);
    master->device = NULL;
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Reserviert einen bestimmten EtherCAT-Master und das zugehörige Gerät.

   Gibt einen Zeiger auf den reservierten EtherCAT-Master zurueck.

   \return Zeiger auf EtherCAT-Master oder NULL, wenn Parameter ungueltig.
*/

ec_master_t *ecrt_request_master(unsigned int master_index
                                 /**< EtherCAT-Master-Index */
                                 )
{
    ec_master_t *master;

    EC_INFO("Requesting master %i...\n", master_index);

    if (!(master = ec_find_master(master_index))) goto req_return;

    // begin critical section
    if (master->reserved) {
        EC_ERR("Master %i is already in use!\n", master_index);
        goto req_return;
    }
    master->reserved = 1;
    // end critical section

    if (!master->device) {
        EC_ERR("Master %i has no assigned device!\n", master_index);
        goto req_release;
    }

    if (!try_module_get(master->device->module)) {
        EC_ERR("Failed to reserve device module!\n");
        goto req_release;
    }

    if (ec_device_open(master->device)) {
        EC_ERR("Failed to open device!\n");
        goto req_module_put;
    }

    if (!master->device->link_state) EC_WARN("Link is DOWN.\n");

    if (ec_master_bus_scan(master)) {
        EC_ERR("Bus scan failed!\n");
        goto req_close;
    }

    EC_INFO("Master %i is ready.\n", master_index);
    return master;

 req_close:
    if (ec_device_close(master->device))
        EC_WARN("Warning - Failed to close device!\n");
 req_module_put:
    module_put(master->device->module);
    ec_master_reset(master);
 req_release:
    master->reserved = 0;
 req_return:
    EC_ERR("Failed requesting master %i.\n", master_index);
    return NULL;
}

/*****************************************************************************/

/**
   Gibt einen zuvor angeforderten EtherCAT-Master wieder frei.
*/

void ecrt_release_master(ec_master_t *master /**< EtherCAT-Master */)
{
    EC_INFO("Releasing master %i...\n", master->index);

    if (!master->reserved) {
        EC_ERR("Master %i was never requested!\n", master->index);
        return;
    }

    ec_master_reset(master);

    if (ec_device_close(master->device))
        EC_WARN("Warning - Failed to close device!\n");

    module_put(master->device->module);
    master->reserved = 0;

    EC_INFO("Released master %i.\n", master->index);
    return;
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.
*/

void ec_print_data(const uint8_t *data, size_t size)
{
    unsigned int i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        printk("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus, differentiell.
*/

void ec_print_data_diff(const uint8_t *d1, /**< Daten 1 */
                        const uint8_t *d2, /**< Daten 2 */
                        size_t size /** Anzahl Bytes */
                        )
{
    unsigned int i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        if (d1[i] == d2[i]) printk(".. ");
        else printk("%02X ", d2[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(ecdev_register);
EXPORT_SYMBOL(ecdev_unregister);
EXPORT_SYMBOL(ecrt_request_master);
EXPORT_SYMBOL(ecrt_release_master);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
