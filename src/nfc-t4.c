/**
 * Depending on the type of package, there are different
 * compilation rules for this directory.  This comment applies
 * to packages of type "pkg."  For other types of packages,
 * please view the documentation at http://mynewt.apache.org/.
 *
 * Put source files in this directory.  All files that have a *.c
 * ending are recursively compiled in the src/ directory and its
 * descendants.  The exception here is the arch/ directory, which
 * is ignored in the default compilation.
 *
 * The arch/<your-arch>/ directories are manually added and
 * recursively compiled for all files that end with either *.c
 * or *.a.  Any directories in arch/ that don't match the
 * architecture being compiled are not compiled.
 *
 * Architecture is set by the BSP/MCU combination.
 */

#include <assert.h>

#include "os/os.h"
#include "ble/ble.h"
#include "nfc-t4/nfc_t4t_lib.h"
#include "nfc-t4/nfc_uri_msg.h"

#if MYNEWT_VAL(NFC_PINS_AS_GPIO) == 1
    #error "Must set NFC_PINS_AS_GPIO = 0"
#endif

#define NDEF_FILE_SIZE 1024
static uint8_t m_ndef_msg_buf[NDEF_FILE_SIZE];      /**< Buffer for NDEF file. */

static const uint8_t url[] = "helmpcb.com";

static void generate_nfc_tag(void) {
    ret_code_t rc;
	uint32_t len = sizeof(m_ndef_msg_buf);

    nfc_t4t_emulation_stop();

	rc = nfc_uri_msg_encode( NFC_URI_HTTPS,
							 url,
							 sizeof(url) - 1,
							 m_ndef_msg_buf,
							 &len);
	assert(rc == NRF_SUCCESS);

    /* Start the tag emulation */
    rc = nfc_t4t_ndef_rwpayload_set(m_ndef_msg_buf, sizeof(m_ndef_msg_buf));
    assert(rc == NRF_SUCCESS);

    rc = nfc_t4t_emulation_start();
    assert(rc == NRF_SUCCESS);
}

static void nfc_update_tag_cb(struct os_event *ev)
{
    generate_nfc_tag();
}

static struct os_event nfc_update_tag_event = {
    .ev_cb = nfc_update_tag_cb,
};

static struct os_callout reset_tag_timer;

void nfc_update_tag(void)
{
    os_eventq_put(os_eventq_dflt_get(), &nfc_update_tag_event);    
}

static void nfc_read_event_cb(struct os_event *ev)
{
    ble_advertise();
}

static struct os_event nfc_read_event = {
    .ev_cb = nfc_read_event_cb,
};

static void nfc_write_event_cb(struct os_event *ev)
{

}

static uint32_t nfc_write_data_length;

static struct os_event nfc_write_event = {
    .ev_cb = nfc_write_event_cb,
    .ev_arg = &nfc_write_data_length,
};

static void nfc_callback(void          * context,
                         nfc_t4t_event_t event,
                         const uint8_t * data,
                         size_t          dataLength,
                         uint32_t        flags)
{
    switch (event)
    {
        case NFC_T4T_EVENT_FIELD_ON:
            break;

        case NFC_T4T_EVENT_FIELD_OFF:
            break;

        case NFC_T4T_EVENT_NDEF_READ:
            os_eventq_put(os_eventq_dflt_get(), &nfc_read_event);
            break;

        case NFC_T4T_EVENT_NDEF_UPDATED:
            if (dataLength) {
                nfc_write_data_length = dataLength;
                os_eventq_put(os_eventq_dflt_get(), &nfc_write_event);
            }
            break;

        default:
            break;
    }
}

void nfc_t4_init(void) {
    ret_code_t ret_code;

    os_callout_init(&reset_tag_timer, os_eventq_dflt_get(), nfc_update_tag_cb, NULL);

    ret_code = nfc_t4t_setup(nfc_callback, NULL);
    assert(ret_code == NRF_SUCCESS);

    nfc_update_tag();
}