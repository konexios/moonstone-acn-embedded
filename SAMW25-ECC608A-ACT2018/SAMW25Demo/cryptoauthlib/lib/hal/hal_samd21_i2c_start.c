/**
 * \file
 * \brief ATCA Hardware abstraction layer for SAMD21 I2C over START drivers.
 *
 * This code is structured in two parts.  Part 1 is the connection of the ATCA HAL API to the physical I2C
 * implementation. Part 2 is the START I2C primitives to set up the interface.
 *
 * Prerequisite: add SERCOM I2C Master Polled support to application in Atmel Studio
 *
 * \copyright (c) 2017 Microchip Technology Inc. and its subsidiaries.
 *            You may use this software and any derivatives exclusively with
 *            Microchip products.
 *
 * \page License
 *
 * (c) 2017 Microchip Technology Inc. and its subsidiaries. You may use this
 * software and any derivatives exclusively with Microchip products.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 * PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION
 * WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
 *
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIPS TOTAL LIABILITY ON ALL CLAIMS IN
 * ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE
 * TERMS.
 */
 #include <string.h>
#include <stdio.h>
#include <hal_gpio.h>
#include <hal_delay.h>
#include <cryptoauthlib.h>
#include "atca_hal.h"
#include "atca_device.h"
#include "driver_init.h"
#include "hal_samd21_i2c_start.h"
#include "peripheral_clk_config.h"
#include "atca_execution.h"

#include "atca_start_config.h"
#include "atca_start_iface.h"

/** \defgroup hal_ Hardware abstraction layer (hal_)
 *
 * \brief
 * These methods define the hardware abstraction layer for communicating with a CryptoAuth device
 *
   @{ */

ATCAI2CMaster_t *i2c_hal_data[MAX_I2C_BUSES];   // map logical, 0-based bus number to index
int i2c_bus_ref_ct = 0;                         // total in-use count across buses

/* Notes:
    - this HAL implementation assumes you've included the Atmel START SERCOM I2C libraries in your project, otherwise,
    the HAL layer will not compile because the START I2C drivers are a dependency *
 */

/** \brief discover i2c buses available for this hardware
 * this maintains a list of logical to physical bus mappings freeing the application
 * of the a-priori knowledge
 * \param[in] i2c_buses - an array of logical bus numbers
 * \param[in] max_buses - maximum number of buses the app wants to attempt to discover
 * \return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_discover_buses(int i2c_buses[], int max_buses)
{

    /* if every SERCOM was a likely candidate bus, then would need to initialize the entire array to all SERCOM n numbers.
     * As an optimization and making discovery safer, make assumptions about bus-num / SERCOM map based on D21 Xplained Pro board
     * If you were using a raw D21 on your own board, you would supply your own bus numbers based on your particular hardware configuration.
     */
#ifdef __SAMR21G18A__
    i2c_buses[0] = 1;   // default r21 for xplained pro dev board
#else
    i2c_buses[0] = 2;   // default d21 for xplained pro dev board
#endif

    return ATCA_SUCCESS;
}

/** \brief discover any CryptoAuth devices on a given logical bus number
 * \param[in]  busNum  logical bus number on which to look for CryptoAuth devices
 * \param[out] cfg     pointer to head of an array of interface config structures which get filled in by this method
 * \param[out] found   number of devices found on this bus
 * \return ATCA_SUCCESS
 */

ATCA_STATUS hal_i2c_discover_devices(int busNum, ATCAIfaceCfg cfg[], int *found)
{
    ATCAIfaceCfg *head = cfg;
    uint8_t slaveAddress = 0x01;
    ATCADevice device;
    ATCAPacket packet;
    ATCA_STATUS status;
    uint8_t revs608[][4] = { { 0x00, 0x00, 0x60, 0x01 }, { 0x00, 0x00, 0x60, 0x02 } };
    uint8_t revs508[][4] = { { 0x00, 0x00, 0x50, 0x00 } };
    uint8_t revs108[][4] = { { 0x80, 0x00, 0x10, 0x01 } };
    uint8_t revs204[][4] = { { 0x00, 0x02, 0x00, 0x08 }, { 0x00, 0x02, 0x00, 0x09 }, { 0x00, 0x04, 0x05, 0x00 } };
    int i;

    /** \brief default configuration, to be reused during discovery process */
    ATCAIfaceCfg discoverCfg = {
        .iface_type             = ATCA_I2C_IFACE,
        .devtype                = ATECC508A,
        .atcai2c.slave_address  = 0x07,
        .atcai2c.bus            = busNum,
        .atcai2c.baud           = 400000,
        //.atcai2c.baud = 100000,
        .wake_delay             = 800,
        .rx_retries             = 3
    };

    ATCAHAL_t hal;

    if (busNum < 0)
    {
        return ATCA_COMM_FAIL;
    }

    hal_i2c_init(&hal, &discoverCfg);
    device = newATCADevice(&discoverCfg);

    // iterate through all addresses on given i2c bus
    // all valid 7-bit addresses go from 0x07 to 0x78
    for (slaveAddress = 0x07; slaveAddress <= 0x78; slaveAddress++)
    {
        discoverCfg.atcai2c.slave_address = slaveAddress << 1;  // turn it into an 8-bit address which is what the rest of the i2c HAL is expecting when a packet is sent

        memset(&packet, 0x00, sizeof(packet));
        // build an info command
        packet.param1 = INFO_MODE_REVISION;
        packet.param2 = 0;
        // get devrev info and set device type accordingly
        atInfo(device->mCommands, &packet);
        if ((status = atca_execute_command(&packet, device)) != ATCA_SUCCESS)
        {
            continue;
        }

        // determine device type from common info and dev rev response byte strings... start with unknown
        discoverCfg.devtype = ATCA_DEV_UNKNOWN;

        for (i = 0; i < (int)sizeof(revs608) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs608[i], 4) == 0)
            {
                discoverCfg.devtype = ATECC608A;
                break;
            }
        }

        for (i = 0; i < (int)sizeof(revs508) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs508[i], 4) == 0)
            {
                discoverCfg.devtype = ATECC508A;
                break;
            }
        }

        for (i = 0; i < (int)sizeof(revs204) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs204[i], 4) == 0)
            {
                discoverCfg.devtype = ATSHA204A;
                break;
            }
        }

        for (i = 0; i < (int)sizeof(revs108) / 4; i++)
        {
            if (memcmp(&packet.data[1], &revs108[i], 4) == 0)
            {
                discoverCfg.devtype = ATECC108A;
                break;
            }
        }

        if (discoverCfg.devtype != ATCA_DEV_UNKNOWN)
        {
            // now the device type is known, so update the caller's cfg array element with it
            (*found)++;
            memcpy( (uint8_t*)head, (uint8_t*)&discoverCfg, sizeof(ATCAIfaceCfg));
            head->devtype = discoverCfg.devtype;
            head++;
        }

        atca_delay_ms(15);
    }

    deleteATCADevice(&device);

    return ATCA_SUCCESS;
}



/** \brief
    - this HAL implementation assumes you've included the START Twi libraries in your project, otherwise,
    the HAL layer will not compile because the START TWI drivers are a dependency *
 */

/** \brief hal_i2c_init manages requests to initialize a physical interface.  it manages use counts so when an interface
 * has released the physical layer, it will disable the interface for some other use.
 * You can have multiple ATCAIFace instances using the same bus, and you can have multiple ATCAIFace instances on
 * multiple i2c buses, so hal_i2c_init manages these things and ATCAIFace is abstracted from the physical details.
 */

/** \brief initialize an I2C interface using given config
 * \param[in] hal - opaque ptr to HAL data
 * \param[in] cfg - interface configuration
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_init(void *hal, ATCAIfaceCfg *cfg)
{
    int bus = cfg->atcai2c.bus; // 0-based logical bus number
    ATCAHAL_t *phal = (ATCAHAL_t*)hal;
    uint32_t freq_constant;     // I2C frequency configuration constant in kHz

    if (i2c_bus_ref_ct == 0)    // power up state, no i2c buses will have been used
    {
        for (int i = 0; i < MAX_I2C_BUSES; i++)
        {
            i2c_hal_data[i] = NULL;
        }
    }

    i2c_bus_ref_ct++;  // total across buses

    if (bus >= 0 && bus < MAX_I2C_BUSES)
    {
        // if this is the first time this bus and interface has been created, do the physical work of enabling it
        if (i2c_hal_data[bus] == NULL)
        {
            i2c_hal_data[bus] = malloc(sizeof(ATCAI2CMaster_t) );
            i2c_hal_data[bus]->ref_ct = 1;  // buses are shared, this is the first instance

            // store I2C baudrate in kHz
            freq_constant = cfg->atcai2c.baud / 1000;

            switch (bus)
            {
            case 0: break;
            case 1: break;
            case 2: memcpy(&(i2c_hal_data[bus]->i2c_master_instance), &I2C_0, sizeof(struct i2c_m_sync_desc)); break;
            case 3: break;
            case 4: break;
            case 5: break;
            }

            // store this for use during the release phase
            i2c_hal_data[bus]->bus_index = bus;

            // set I2C baudrate and enable I2C module
            i2c_m_sync_set_baudrate(&(i2c_hal_data[bus]->i2c_master_instance), CONF_GCLK_SERCOM2_CORE_FREQUENCY / 1000, freq_constant);
            i2c_m_sync_enable(&(i2c_hal_data[bus]->i2c_master_instance));

        }
        else
        {
            // otherwise, another interface already initialized the bus, so this interface will share it and any different
            // cfg parameters will be ignored...first one to initialize this sets the configuration
            i2c_hal_data[bus]->ref_ct++;
        }
        phal->hal_data = i2c_hal_data[bus];

        return ATCA_SUCCESS;
    }

    return ATCA_COMM_FAIL;
}

/** \brief HAL implementation of I2C post init
 * \param[in] iface  instance
 * \return ATCA_SUCCESS
 */
ATCA_STATUS hal_i2c_post_init(ATCAIface iface)
{
    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C send over START
 * \param[in] iface     instance
 * \param[in] txdata    pointer to space to bytes to send
 * \param[in] txlength  number of bytes to send
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_send(ATCAIface iface, uint8_t *txdata, int txlength)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int bus = cfg->atcai2c.bus;

    struct _i2c_m_msg packet = {
        .addr   = cfg->atcai2c.slave_address >> 1,
        .buffer = txdata,
        .flags  = I2C_M_SEVEN | I2C_M_STOP,
    };

    // for this implementation of I2C with CryptoAuth chips, txdata is assumed to have ATCAPacket format

    // other device types that don't require i/o tokens on the front end of a command need a different hal_i2c_send and wire it up instead of this one
    // this covers devices such as ATSHA204A and ATECCx08A that require a word address value pre-pended to the packet
    // txdata[0] is using _reserved byte of the ATCAPacket
    txdata[0] = 0x03;   // insert the Word Address Value, Command token
    txlength++;         // account for word address value byte.
    packet.len = txlength;

    if (i2c_m_sync_transfer(&(i2c_hal_data[bus]->i2c_master_instance), &packet) != I2C_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

/** \brief HAL implementation of I2C receive function for START I2C
 * \param[in] iface     instance
 * \param[out] rxdata    pointer to space to receive the data
 * \param[in] rxlength  ptr to expected number of receive bytes to request
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_receive(ATCAIface iface, uint8_t *rxdata, uint16_t *rxlength)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int bus = cfg->atcai2c.bus;
    int retries = cfg->rx_retries;
    int status = !I2C_OK;

    struct _i2c_m_msg packet = {
        .addr   = cfg->atcai2c.slave_address >> 1,
        .len    = *rxlength,
        .buffer = rxdata,
        .flags  = I2C_M_SEVEN | I2C_M_RD | I2C_M_STOP,
    };

    while (retries-- > 0 && status != I2C_OK)
    {
        status = i2c_m_sync_transfer(&(i2c_hal_data[bus]->i2c_master_instance), &packet);
    }

    if (status != I2C_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

/** \brief method to change the bus speec of I2C
 * \param[in] iface  interface on which to change bus speed
 * \param[in] speed  baud rate (typically 100000 or 400000)
 */

void change_i2c_speed(ATCAIface iface, uint32_t speed)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int bus = cfg->atcai2c.bus;
    uint32_t freq_constant; // I2C frequency configuration constant

    // disable I2C module
    i2c_m_sync_disable(&(i2c_hal_data[bus]->i2c_master_instance));

    // store I2C baudrate in kHz
    freq_constant = speed / 1000;

    // set I2C baudrate and enable I2C module
    i2c_m_sync_set_baudrate(&(i2c_hal_data[bus]->i2c_master_instance), CONF_GCLK_SERCOM2_CORE_FREQUENCY / 1000, freq_constant);
    i2c_m_sync_enable(&(i2c_hal_data[bus]->i2c_master_instance));
}

/** \brief wake up CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to wakeup
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_wake(ATCAIface iface)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int bus = cfg->atcai2c.bus;
    int retries = cfg->rx_retries;
    uint32_t bdrt = cfg->atcai2c.baud;
    int status = !I2C_OK;
    uint8_t data[4], expected[4] = { 0x04, 0x11, 0x33, 0x43 };

    if (bdrt != 100000)    // if not already at 100KHz, change it
    {
        change_i2c_speed(iface, 100000);
    }

    // send the wake by writing to an address of 0x00
    struct _i2c_m_msg packet = {
        .addr   = 0x00,
        .len    = 0,
        .buffer = NULL,
        .flags  = I2C_M_SEVEN | I2C_M_STOP,
    };

    // Send the 00 address as the wake pulse
    status = i2c_m_sync_transfer(&(i2c_hal_data[bus]->i2c_master_instance), &packet);   // part will NACK, so don't check for status

    delay_us(cfg->wake_delay);                                                          // wait tWHI + tWLO which is configured based on device type and configuration structure

    // receive the wake up response
    packet.addr = cfg->atcai2c.slave_address >> 1;
    packet.len = 4;
    packet.buffer = data;
    packet.flags  = I2C_M_SEVEN | I2C_M_RD | I2C_M_STOP;

    while (retries-- > 0 && status != I2C_OK)
    {
        status = i2c_m_sync_transfer(&(i2c_hal_data[bus]->i2c_master_instance), &packet);
    }

    if (status == I2C_OK)
    {
        // if necessary, revert baud rate to what came in.
        if (bdrt != 100000)
        {
            change_i2c_speed(iface, bdrt);
        }
        // compare received data with expected value
        if (memcmp(data, expected, 4) == 0)
        {
            return ATCA_SUCCESS;
        }
    }

    return ATCA_COMM_FAIL;
}

/** \brief idle CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to idle
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_idle(ATCAIface iface)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int bus = cfg->atcai2c.bus;
    uint8_t data[4];

    struct _i2c_m_msg packet = {
        .addr   = cfg->atcai2c.slave_address >> 1,
        .len    = 1,
        .buffer = &data[0],
        .flags  = I2C_M_SEVEN | I2C_M_STOP,
    };

    data[0] = 0x02;  // idle word address value

    if (i2c_m_sync_transfer(&(i2c_hal_data[bus]->i2c_master_instance), &packet) != I2C_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

/** \brief sleep CryptoAuth device using I2C bus
 * \param[in] iface  interface to logical device to sleep
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_sleep(ATCAIface iface)
{
    ATCAIfaceCfg *cfg = atgetifacecfg(iface);
    int bus = cfg->atcai2c.bus;
    uint8_t data[4];

    struct _i2c_m_msg packet = {
        .addr   = cfg->atcai2c.slave_address >> 1,
        .len    = 1,
        .buffer = &data[0],
        .flags  = I2C_M_SEVEN | I2C_M_STOP,
    };

    data[0] = 0x01;  // sleep word address value

    if (i2c_m_sync_transfer(&(i2c_hal_data[bus]->i2c_master_instance), &packet) != I2C_OK)
    {
        return ATCA_COMM_FAIL;
    }

    return ATCA_SUCCESS;
}

/** \brief manages reference count on given bus and releases resource if no more refences exist
 * \param[in] hal_data - opaque pointer to hal data structure - known only to the HAL implementation
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */

ATCA_STATUS hal_i2c_release(void *hal_data)
{
    ATCAI2CMaster_t *hal = (ATCAI2CMaster_t*)hal_data;

    i2c_bus_ref_ct--;  // track total i2c bus interface instances for consistency checking and debugging

    //if the use count for this bus has gone to 0 references, disable it.  protect against an unbracketed release
    if (hal && --(hal->ref_ct) <= 0 && i2c_hal_data[hal->bus_index] != NULL)
    {
        i2c_m_sync_disable(&(hal->i2c_master_instance));
        free(i2c_hal_data[hal->bus_index]);
        i2c_hal_data[hal->bus_index] = NULL;
    }

    return ATCA_SUCCESS;
}

/** @} */
