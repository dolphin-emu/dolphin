/*
 * Windows CE backend for libusbx 1.0
 * Copyright © 2011-2013 RealVNC Ltd.
 * Large portions taken from Windows backend, which is
 * Copyright © 2009-2010 Pete Batard <pbatard@gmail.com>
 * With contributions from Michael Plante, Orin Eman et al.
 * Parts of this code adapted from libusb-win32-v1 by Stephan Meyer
 * Major code testing contribution by Xiaofan Chen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libusbi.h>

#include <stdint.h>
#include <errno.h>
#include <inttypes.h>

#include "wince_usb.h"

// Forward declares
static int wince_clock_gettime(int clk_id, struct timespec *tp);
unsigned __stdcall wince_clock_gettime_threaded(void* param);

// Global variables
uint64_t hires_frequency, hires_ticks_to_ps;
int errno;
const uint64_t epoch_time = UINT64_C(116444736000000000);       // 1970.01.01 00:00:000 in MS Filetime
enum windows_version windows_version = WINDOWS_CE;
static int concurrent_usage = -1;
// Timer thread
// NB: index 0 is for monotonic and 1 is for the thread exit event
HANDLE timer_thread = NULL;
HANDLE timer_mutex = NULL;
struct timespec timer_tp;
volatile LONG request_count[2] = {0, 1};	// last one must be > 0
HANDLE timer_request[2] = { NULL, NULL };
HANDLE timer_response = NULL;
HANDLE driver_handle = INVALID_HANDLE_VALUE;

/*
 * Converts a windows error to human readable string
 * uses retval as errorcode, or, if 0, use GetLastError()
 */
#if defined(ENABLE_LOGGING)
static char* windows_error_str(uint32_t retval)
{
	static TCHAR wErr_string[ERR_BUFFER_SIZE];
	static char err_string[ERR_BUFFER_SIZE];

	DWORD size;
	size_t i;
	uint32_t error_code, format_error;

	error_code = retval?retval:GetLastError();
	
	safe_stprintf(wErr_string, ERR_BUFFER_SIZE, _T("[%d] "), error_code);
	
	size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), &wErr_string[safe_tcslen(wErr_string)],
		ERR_BUFFER_SIZE - (DWORD)safe_tcslen(wErr_string), NULL);
	if (size == 0) {
		format_error = GetLastError();
		if (format_error)
			safe_stprintf(wErr_string, ERR_BUFFER_SIZE,
				_T("Windows error code %u (FormatMessage error code %u)"), error_code, format_error);
		else
			safe_stprintf(wErr_string, ERR_BUFFER_SIZE, _T("Unknown error code %u"), error_code);
	} else {
		// Remove CR/LF terminators
		for (i=safe_tcslen(wErr_string)-1; ((wErr_string[i]==0x0A) || (wErr_string[i]==0x0D)); i--) {
			wErr_string[i] = 0;
		}
	}
	if (WideCharToMultiByte(CP_ACP, 0, wErr_string, -1, err_string, ERR_BUFFER_SIZE, NULL, NULL) < 0)
	{
		strcpy(err_string, "Unable to convert error string");
	}
	return err_string;
}
#endif

static struct wince_device_priv *_device_priv(struct libusb_device *dev)
{
        return (struct wince_device_priv *) dev->os_priv;
}

// ceusbkwrapper to libusb error code mapping
static int translate_driver_error(int error) 
{
	switch (error) {
		case ERROR_INVALID_PARAMETER:
			return LIBUSB_ERROR_INVALID_PARAM;
		case ERROR_CALL_NOT_IMPLEMENTED:
		case ERROR_NOT_SUPPORTED:
			return LIBUSB_ERROR_NOT_SUPPORTED;
		case ERROR_NOT_ENOUGH_MEMORY:
			return LIBUSB_ERROR_NO_MEM;
		case ERROR_INVALID_HANDLE:
			return LIBUSB_ERROR_NO_DEVICE;
		case ERROR_BUSY:
			return LIBUSB_ERROR_BUSY;

		// Error codes that are either unexpected, or have 
		// no suitable LIBUSB_ERROR equivilant.
		case ERROR_CANCELLED:
		case ERROR_INTERNAL_ERROR:
		default:
			return LIBUSB_ERROR_OTHER;
	}
}

static int init_dllimports()
{
	DLL_LOAD(ceusbkwrapper.dll, UkwOpenDriver, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwGetDeviceList, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwReleaseDeviceList, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwGetDeviceAddress, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwGetDeviceDescriptor, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwGetConfigDescriptor, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwCloseDriver, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwCancelTransfer, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwIssueControlTransfer, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwClaimInterface, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwReleaseInterface, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwSetInterfaceAlternateSetting, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwClearHaltHost, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwClearHaltDevice, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwGetConfig, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwSetConfig, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwResetDevice, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwKernelDriverActive, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwAttachKernelDriver, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwDetachKernelDriver, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwIssueBulkTransfer, TRUE);
	DLL_LOAD(ceusbkwrapper.dll, UkwIsPipeHalted, TRUE);
	return LIBUSB_SUCCESS;
}

static int init_device(struct libusb_device *dev, UKW_DEVICE drv_dev,
					   unsigned char bus_addr, unsigned char dev_addr)
{
	struct wince_device_priv *priv = _device_priv(dev);
	int r = LIBUSB_SUCCESS;

	dev->bus_number = bus_addr;
	dev->device_address = dev_addr;
	priv->dev = drv_dev;

	if (!UkwGetDeviceDescriptor(priv->dev, &(priv->desc))) {
		r = translate_driver_error(GetLastError());
	}
	return r;
}

// Internal API functions
static int wince_init(struct libusb_context *ctx)
{
	int i, r = LIBUSB_ERROR_OTHER;
	HANDLE semaphore;
	TCHAR sem_name[11+1+8]; // strlen(libusb_init)+'\0'+(32-bit hex PID)

	_stprintf(sem_name, _T("libusb_init%08X"), (unsigned int)GetCurrentProcessId()&0xFFFFFFFF);
	semaphore = CreateSemaphore(NULL, 1, 1, sem_name);
	if (semaphore == NULL) {
		usbi_err(ctx, "could not create semaphore: %s", windows_error_str(0));
		return LIBUSB_ERROR_NO_MEM;
	}

	// A successful wait brings our semaphore count to 0 (unsignaled)
	// => any concurent wait stalls until the semaphore's release
	if (WaitForSingleObject(semaphore, INFINITE) != WAIT_OBJECT_0) {
		usbi_err(ctx, "failure to access semaphore: %s", windows_error_str(0));
		CloseHandle(semaphore);
		return LIBUSB_ERROR_NO_MEM;
	}

	// NB: concurrent usage supposes that init calls are equally balanced with
	// exit calls. If init is called more than exit, we will not exit properly
	if ( ++concurrent_usage == 0 ) {	// First init?
		// Initialize pollable file descriptors
		init_polling();

		// Load DLL imports
		if (init_dllimports() != LIBUSB_SUCCESS) {
			usbi_err(ctx, "could not resolve DLL functions");
			r = LIBUSB_ERROR_NOT_SUPPORTED;
			goto init_exit;
		}

		// try to open a handle to the driver
		driver_handle = UkwOpenDriver();
		if (driver_handle == INVALID_HANDLE_VALUE) {
			usbi_err(ctx, "could not connect to driver");
			r = LIBUSB_ERROR_NOT_SUPPORTED;
			goto init_exit;
		}

		// Windows CE doesn't have a way of specifying thread affinity, so this code
		// just has  to hope QueryPerformanceCounter doesn't report different values when
		// running on different cores.
		r = LIBUSB_ERROR_NO_MEM;
		for (i = 0; i < 2; i++) {
			timer_request[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (timer_request[i] == NULL) {
				usbi_err(ctx, "could not create timer request event %d - aborting", i);
				goto init_exit;
			}
		}
		timer_response = CreateSemaphore(NULL, 0, MAX_TIMER_SEMAPHORES, NULL);
		if (timer_response == NULL) {
			usbi_err(ctx, "could not create timer response semaphore - aborting");
			goto init_exit;
		}
		timer_mutex = CreateMutex(NULL, FALSE, NULL);
		if (timer_mutex == NULL) {
			usbi_err(ctx, "could not create timer mutex - aborting");
			goto init_exit;
		}
		timer_thread = CreateThread(NULL, 0, wince_clock_gettime_threaded, NULL, 0, NULL);
		if (timer_thread == NULL) {
			usbi_err(ctx, "Unable to create timer thread - aborting");
			goto init_exit;
		}
	}
	// At this stage, either we went through full init successfully, or didn't need to
	r = LIBUSB_SUCCESS;

init_exit: // Holds semaphore here.
	if (!concurrent_usage && r != LIBUSB_SUCCESS) { // First init failed?
		if (driver_handle != INVALID_HANDLE_VALUE) {
			UkwCloseDriver(driver_handle);
			driver_handle = INVALID_HANDLE_VALUE;
		}
		if (timer_thread) {
			SetEvent(timer_request[1]); // actually the signal to quit the thread.
			if (WAIT_OBJECT_0 != WaitForSingleObject(timer_thread, INFINITE)) {
				usbi_warn(ctx, "could not wait for timer thread to quit");
				TerminateThread(timer_thread, 1); // shouldn't happen, but we're destroying
												  // all objects it might have held anyway.
			}
			CloseHandle(timer_thread);
			timer_thread = NULL;
		}
		for (i = 0; i < 2; i++) {
			if (timer_request[i]) {
				CloseHandle(timer_request[i]);
				timer_request[i] = NULL;
			}
		}
		if (timer_response) {
			CloseHandle(timer_response);
			timer_response = NULL;
		}
		if (timer_mutex) {
			CloseHandle(timer_mutex);
			timer_mutex = NULL;
		}
	}

	if (r != LIBUSB_SUCCESS)
		--concurrent_usage; // Not expected to call libusb_exit if we failed.

	ReleaseSemaphore(semaphore, 1, NULL);	// increase count back to 1
	CloseHandle(semaphore);
	return r;
}

static void wince_exit(void)
{
	int i;
	HANDLE semaphore;
	TCHAR sem_name[11+1+8]; // strlen(libusb_init)+'\0'+(32-bit hex PID)

	_stprintf(sem_name, _T("libusb_init%08X"), (unsigned int)GetCurrentProcessId()&0xFFFFFFFF);
	semaphore = CreateSemaphore(NULL, 1, 1, sem_name);
	if (semaphore == NULL) {
		return;
	}

	// A successful wait brings our semaphore count to 0 (unsignaled)
	// => any concurent wait stalls until the semaphore release
	if (WaitForSingleObject(semaphore, INFINITE) != WAIT_OBJECT_0) {
		CloseHandle(semaphore);
		return;
	}

	// Only works if exits and inits are balanced exactly
	if (--concurrent_usage < 0) {	// Last exit
		exit_polling();

		if (timer_thread) {
			SetEvent(timer_request[1]); // actually the signal to quit the thread.
			if (WAIT_OBJECT_0 != WaitForSingleObject(timer_thread, INFINITE)) {
				usbi_dbg("could not wait for timer thread to quit");
				TerminateThread(timer_thread, 1);
			}
			CloseHandle(timer_thread);
			timer_thread = NULL;
		}
		for (i = 0; i < 2; i++) {
			if (timer_request[i]) {
				CloseHandle(timer_request[i]);
				timer_request[i] = NULL;
			}
		}
		if (timer_response) {
			CloseHandle(timer_response);
			timer_response = NULL;
		}
		if (timer_mutex) {
			CloseHandle(timer_mutex);
			timer_mutex = NULL;
		}
		if (driver_handle != INVALID_HANDLE_VALUE) {
			UkwCloseDriver(driver_handle);
			driver_handle = INVALID_HANDLE_VALUE;
		}
	}

	ReleaseSemaphore(semaphore, 1, NULL);	// increase count back to 1
	CloseHandle(semaphore);
}

static int wince_get_device_list(
	struct libusb_context *ctx,
	struct discovered_devs **discdevs)
{
	UKW_DEVICE devices[MAX_DEVICE_COUNT];
	struct discovered_devs * new_devices = *discdevs;
	DWORD count = 0, i;
	struct libusb_device *dev = NULL;
	unsigned char bus_addr, dev_addr;
	unsigned long session_id;
	BOOL success;
	DWORD release_list_offset = 0;
	int r = LIBUSB_SUCCESS;

	success = UkwGetDeviceList(driver_handle, devices, MAX_DEVICE_COUNT, &count);
	if (!success) {
		int libusbErr = translate_driver_error(GetLastError());
		usbi_err(ctx, "could not get devices: %s", windows_error_str(0));
		return libusbErr;
	}
	for(i = 0; i < count; ++i) {
		release_list_offset = i;
		success = UkwGetDeviceAddress(devices[i], &bus_addr, &dev_addr, &session_id);
		if (!success) {
			r = translate_driver_error(GetLastError());
			usbi_err(ctx, "could not get device address for %d: %s", i, windows_error_str(0));
			goto err_out;
		}
		dev = usbi_get_device_by_session_id(ctx, session_id);
		if (dev) {
			usbi_dbg("using existing device for %d/%d (session %ld)",
					bus_addr, dev_addr, session_id);
			libusb_ref_device(dev);
			// Release just this element in the device list (as we already hold a 
			// reference to it).
			UkwReleaseDeviceList(driver_handle, &devices[i], 1);
			release_list_offset++;
		} else {
			usbi_dbg("allocating new device for %d/%d (session %ld)",
					bus_addr, dev_addr, session_id);
			dev = usbi_alloc_device(ctx, session_id);
			if (!dev) {
				r = LIBUSB_ERROR_NO_MEM;
				goto err_out;
			}
			r = init_device(dev, devices[i], bus_addr, dev_addr);
			if (r < 0)
				goto err_out;
			r = usbi_sanitize_device(dev);
			if (r < 0)
				goto err_out;
		}
		new_devices = discovered_devs_append(new_devices, dev);
		if (!discdevs) {
			r = LIBUSB_ERROR_NO_MEM;
			goto err_out;
		}
		safe_unref_device(dev);
	}
	*discdevs = new_devices;
	return r;
err_out:
	*discdevs = new_devices;
	safe_unref_device(dev);
	// Release the remainder of the unprocessed device list.
	// The devices added to new_devices already will still be passed up to libusb, 
	// which can dispose of them at its leisure.
	UkwReleaseDeviceList(driver_handle, &devices[release_list_offset], count - release_list_offset);
	return r;
}

static int wince_open(struct libusb_device_handle *handle)
{
	// Nothing to do to open devices as a handle to it has
	// been retrieved by wince_get_device_list
	return LIBUSB_SUCCESS;
}

static void wince_close(struct libusb_device_handle *handle)
{
	// Nothing to do as wince_open does nothing.
}

static int wince_get_device_descriptor(
   struct libusb_device *device,
   unsigned char *buffer, int *host_endian)
{
	struct wince_device_priv *priv = _device_priv(device);

	*host_endian = 1;
	memcpy(buffer, &priv->desc, DEVICE_DESC_LENGTH);
	return LIBUSB_SUCCESS;
}

static int wince_get_active_config_descriptor(
	struct libusb_device *device,
	unsigned char *buffer, size_t len, int *host_endian)
{
	struct wince_device_priv *priv = _device_priv(device);
	DWORD actualSize = len;
	*host_endian = 0;
	if (!UkwGetConfigDescriptor(priv->dev, UKW_ACTIVE_CONFIGURATION, buffer, len, &actualSize)) {
		return translate_driver_error(GetLastError());
	}
	return actualSize;
}

static int wince_get_config_descriptor(
	struct libusb_device *device,
	uint8_t config_index,
	unsigned char *buffer, size_t len, int *host_endian)
{
	struct wince_device_priv *priv = _device_priv(device);
	DWORD actualSize = len;
	*host_endian = 0;
	if (!UkwGetConfigDescriptor(priv->dev, config_index, buffer, len, &actualSize)) {
		return translate_driver_error(GetLastError());
	}
	return actualSize;
}

static int wince_get_configuration(
   struct libusb_device_handle *handle,
   int *config)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	UCHAR cv = 0;
	if (!UkwGetConfig(priv->dev, &cv)) {
		return translate_driver_error(GetLastError());
	}
	(*config) = cv;
	return LIBUSB_SUCCESS;
}

static int wince_set_configuration(
	struct libusb_device_handle *handle,
	int config)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	// Setting configuration 0 places the device in Address state.
	// This should correspond to the "unconfigured state" required by
	// libusb when the specified configuration is -1.
	UCHAR cv = (config < 0) ? 0 : config;
	if (!UkwSetConfig(priv->dev, cv)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_claim_interface(
	struct libusb_device_handle *handle,
	int interface_number)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwClaimInterface(priv->dev, interface_number)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_release_interface(
	struct libusb_device_handle *handle,
	int interface_number)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwSetInterfaceAlternateSetting(priv->dev, interface_number, 0)) {
		return translate_driver_error(GetLastError());
	}
	if (!UkwReleaseInterface(priv->dev, interface_number)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_set_interface_altsetting(
	struct libusb_device_handle *handle,
	int interface_number, int altsetting)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwSetInterfaceAlternateSetting(priv->dev, interface_number, altsetting)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_clear_halt(
	struct libusb_device_handle *handle,
	unsigned char endpoint)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwClearHaltHost(priv->dev, endpoint)) {
		return translate_driver_error(GetLastError());
	}
	if (!UkwClearHaltDevice(priv->dev, endpoint)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_reset_device(
	struct libusb_device_handle *handle)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwResetDevice(priv->dev)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_kernel_driver_active(
	struct libusb_device_handle *handle,
	int interface_number)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	BOOL result = FALSE;
	if (!UkwKernelDriverActive(priv->dev, interface_number, &result)) {
		return translate_driver_error(GetLastError());
	}
	return result ? 1 : 0;
}

static int wince_detach_kernel_driver(
	struct libusb_device_handle *handle,
	int interface_number)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwDetachKernelDriver(priv->dev, interface_number)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_attach_kernel_driver(
	struct libusb_device_handle *handle,
	int interface_number)
{
	struct wince_device_priv *priv = _device_priv(handle->dev);
	if (!UkwAttachKernelDriver(priv->dev, interface_number)) {
		return translate_driver_error(GetLastError());
	}	
	return LIBUSB_SUCCESS;
}

static void wince_destroy_device(
	struct libusb_device *dev)
{
	struct wince_device_priv *priv = _device_priv(dev);
	UkwReleaseDeviceList(driver_handle, &priv->dev, 1);
}

static void wince_clear_transfer_priv(
	struct usbi_transfer *itransfer)
{
	struct wince_transfer_priv *transfer_priv = (struct wince_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct winfd wfd = fd_to_winfd(transfer_priv->pollable_fd.fd);
	// No need to cancel transfer as it is either complete or abandoned
	wfd.itransfer = NULL;
	CloseHandle(wfd.handle);
	usbi_free_fd(&transfer_priv->pollable_fd);
}

static int wince_cancel_transfer(
	struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct wince_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	struct wince_transfer_priv *transfer_priv = (struct wince_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	
	if (!UkwCancelTransfer(priv->dev, transfer_priv->pollable_fd.overlapped, UKW_TF_NO_WAIT)) {
		return translate_driver_error(GetLastError());
	}
	return LIBUSB_SUCCESS;
}

static int wince_submit_control_or_bulk_transfer(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct wince_transfer_priv *transfer_priv = (struct wince_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct wince_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	BOOL direction_in, ret;
	struct winfd wfd;
	DWORD flags;
	HANDLE eventHandle;
	PUKW_CONTROL_HEADER setup = NULL;
	const BOOL control_transfer = transfer->type == LIBUSB_TRANSFER_TYPE_CONTROL;

	transfer_priv->pollable_fd = INVALID_WINFD;
	if (control_transfer) {
		setup = (PUKW_CONTROL_HEADER) transfer->buffer;
		direction_in = setup->bmRequestType & LIBUSB_ENDPOINT_IN;
	} else {
		direction_in = transfer->endpoint & LIBUSB_ENDPOINT_IN;
	}
	flags = direction_in ? UKW_TF_IN_TRANSFER : UKW_TF_OUT_TRANSFER;
	flags |= UKW_TF_SHORT_TRANSFER_OK;

	eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (eventHandle == NULL) {
		usbi_err(ctx, "Failed to create event for async transfer");
		return LIBUSB_ERROR_NO_MEM;
	}

	wfd = usbi_create_fd(eventHandle, direction_in ? RW_READ : RW_WRITE, itransfer, &wince_cancel_transfer);
	if (wfd.fd < 0) {
		CloseHandle(eventHandle);
		return LIBUSB_ERROR_NO_MEM;
	}

	transfer_priv->pollable_fd = wfd;
	if (control_transfer) {
		// Split out control setup header and data buffer
		DWORD bufLen = transfer->length - sizeof(UKW_CONTROL_HEADER);
		PVOID buf = (PVOID) &transfer->buffer[sizeof(UKW_CONTROL_HEADER)];

		ret = UkwIssueControlTransfer(priv->dev, flags, setup, buf, bufLen, &transfer->actual_length, wfd.overlapped);
	} else {
		ret = UkwIssueBulkTransfer(priv->dev, flags, transfer->endpoint, transfer->buffer, 
			transfer->length, &transfer->actual_length, wfd.overlapped);
	}
	if (!ret) {
		int libusbErr = translate_driver_error(GetLastError());
		usbi_err(ctx, "UkwIssue%sTransfer failed: error %d",
			control_transfer ? "Control" : "Bulk", GetLastError());
		wince_clear_transfer_priv(itransfer);
		return libusbErr;
	}
	usbi_add_pollfd(ctx, transfer_priv->pollable_fd.fd, direction_in ? POLLIN : POLLOUT);
	itransfer->flags |= USBI_TRANSFER_UPDATED_FDS;

	return LIBUSB_SUCCESS;
}

static int wince_submit_iso_transfer(struct usbi_transfer *itransfer)
{
	return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int wince_submit_transfer(
	struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

	switch (transfer->type) {
	case LIBUSB_TRANSFER_TYPE_CONTROL:
	case LIBUSB_TRANSFER_TYPE_BULK:
	case LIBUSB_TRANSFER_TYPE_INTERRUPT:
		return wince_submit_control_or_bulk_transfer(itransfer);
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		return wince_submit_iso_transfer(itransfer);
	default:
		usbi_err(TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
		return LIBUSB_ERROR_INVALID_PARAM;
	}
}

static void wince_transfer_callback(struct usbi_transfer *itransfer, uint32_t io_result, uint32_t io_size)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct wince_transfer_priv *transfer_priv = (struct wince_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct wince_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int status;

	usbi_dbg("handling I/O completion with errcode %d", io_result);

	if (io_result == ERROR_NOT_SUPPORTED && 
		transfer->type != LIBUSB_TRANSFER_TYPE_CONTROL) {
		/* For functional stalls, the WinCE USB layer (and therefore the USB Kernel Wrapper 
		 * Driver) will report USB_ERROR_STALL/ERROR_NOT_SUPPORTED in situations where the 
		 * endpoint isn't actually stalled.
		 *
		 * One example of this is that some devices will occasionally fail to reply to an IN
		 * token. The WinCE USB layer carries on with the transaction until it is completed
		 * (or cancelled) but then completes it with USB_ERROR_STALL.
		 *
		 * This code therefore needs to confirm that there really is a stall error, by both
		 * checking the pipe status and requesting the endpoint status from the device.
		 */
		BOOL halted = FALSE;
		usbi_dbg("checking I/O completion with errcode ERROR_NOT_SUPPORTED is really a stall");
		if (UkwIsPipeHalted(priv->dev, transfer->endpoint, &halted)) {
			/* Pipe status retrieved, so now request endpoint status by sending a GET_STATUS
			 * control request to the device. This is done synchronously, which is a bit 
			 * naughty, but this is a special corner case.
			 */
			WORD wStatus = 0;
			DWORD written = 0;
			UKW_CONTROL_HEADER ctrlHeader;
			ctrlHeader.bmRequestType = LIBUSB_REQUEST_TYPE_STANDARD |
				LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_ENDPOINT;
			ctrlHeader.bRequest = LIBUSB_REQUEST_GET_STATUS;
			ctrlHeader.wValue = 0;
			ctrlHeader.wIndex = transfer->endpoint;
			ctrlHeader.wLength = sizeof(wStatus);
			if (UkwIssueControlTransfer(priv->dev,
					UKW_TF_IN_TRANSFER | UKW_TF_SEND_TO_ENDPOINT,
					&ctrlHeader, &wStatus, sizeof(wStatus), &written, NULL)) {
				if (written == sizeof(wStatus) &&
						(wStatus & STATUS_HALT_FLAG) == 0) {
					if (!halted || UkwClearHaltHost(priv->dev, transfer->endpoint)) {
						usbi_dbg("Endpoint doesn't appear to be stalled, overriding error with success");
						io_result = ERROR_SUCCESS;
					} else {
						usbi_dbg("Endpoint doesn't appear to be stalled, but the host is halted, changing error");
						io_result = ERROR_IO_DEVICE;
					}
				}
			}
		}
	}

	switch(io_result) {
	case ERROR_SUCCESS:
		itransfer->transferred += io_size;
		status = LIBUSB_TRANSFER_COMPLETED;
		break;
	case ERROR_CANCELLED:
		usbi_dbg("detected transfer cancel");
		status = LIBUSB_TRANSFER_CANCELLED;
		break;
	case ERROR_NOT_SUPPORTED:
	case ERROR_GEN_FAILURE:
		usbi_dbg("detected endpoint stall");
		status = LIBUSB_TRANSFER_STALL;
		break;
	case ERROR_SEM_TIMEOUT:
		usbi_dbg("detected semaphore timeout");
		status = LIBUSB_TRANSFER_TIMED_OUT;
		break;
	case ERROR_OPERATION_ABORTED:
		if (itransfer->flags & USBI_TRANSFER_TIMED_OUT) {
			usbi_dbg("detected timeout");
			status = LIBUSB_TRANSFER_TIMED_OUT;
		} else {
			usbi_dbg("detected operation aborted");
			status = LIBUSB_TRANSFER_CANCELLED;
		}
		break;
	default:
		usbi_err(ITRANSFER_CTX(itransfer), "detected I/O error: %s", windows_error_str(io_result));
		status = LIBUSB_TRANSFER_ERROR;
		break;
	}
	wince_clear_transfer_priv(itransfer);
	if (status == LIBUSB_TRANSFER_CANCELLED) {
		usbi_handle_transfer_cancellation(itransfer);
	} else {
		usbi_handle_transfer_completion(itransfer, (enum libusb_transfer_status)status);
	}
}

static void wince_handle_callback (struct usbi_transfer *itransfer, uint32_t io_result, uint32_t io_size)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

	switch (transfer->type) {
	case LIBUSB_TRANSFER_TYPE_CONTROL:
	case LIBUSB_TRANSFER_TYPE_BULK:
	case LIBUSB_TRANSFER_TYPE_INTERRUPT:
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		wince_transfer_callback (itransfer, io_result, io_size);
		break;
	default:
		usbi_err(ITRANSFER_CTX(itransfer), "unknown endpoint type %d", transfer->type);
	}
}

static int wince_handle_events(
	struct libusb_context *ctx,
	struct pollfd *fds, POLL_NFDS_TYPE nfds, int num_ready)
{
	struct wince_transfer_priv* transfer_priv = NULL;
	POLL_NFDS_TYPE i = 0;
	BOOL found = FALSE;
	struct usbi_transfer *transfer;
	DWORD io_size, io_result;

	usbi_mutex_lock(&ctx->open_devs_lock);
	for (i = 0; i < nfds && num_ready > 0; i++) {

		usbi_dbg("checking fd %d with revents = %04x", fds[i].fd, fds[i].revents);

		if (!fds[i].revents) {
			continue;
		}

		num_ready--;

		// Because a Windows OVERLAPPED is used for poll emulation,
		// a pollable fd is created and stored with each transfer
		usbi_mutex_lock(&ctx->flying_transfers_lock);
		list_for_each_entry(transfer, &ctx->flying_transfers, list, struct usbi_transfer) {
			transfer_priv = usbi_transfer_get_os_priv(transfer);
			if (transfer_priv->pollable_fd.fd == fds[i].fd) {
				found = TRUE;
				break;
			}
		}
		usbi_mutex_unlock(&ctx->flying_transfers_lock);

		if (found && HasOverlappedIoCompleted(transfer_priv->pollable_fd.overlapped)) {
			io_result = (DWORD)transfer_priv->pollable_fd.overlapped->Internal;
			io_size = (DWORD)transfer_priv->pollable_fd.overlapped->InternalHigh;
			usbi_remove_pollfd(ctx, transfer_priv->pollable_fd.fd);
			// let handle_callback free the event using the transfer wfd
			// If you don't use the transfer wfd, you run a risk of trying to free a
			// newly allocated wfd that took the place of the one from the transfer.
			wince_handle_callback(transfer, io_result, io_size);
		} else if (found) {
			usbi_err(ctx, "matching transfer for fd %x has not completed", fds[i]);
			return LIBUSB_ERROR_OTHER;
		} else {
			usbi_err(ctx, "could not find a matching transfer for fd %x", fds[i]);
			return LIBUSB_ERROR_NOT_FOUND;
		}
	}

	usbi_mutex_unlock(&ctx->open_devs_lock);
	return LIBUSB_SUCCESS;
}

/*
 * Monotonic and real time functions
 */
unsigned __stdcall wince_clock_gettime_threaded(void* param)
{
	LARGE_INTEGER hires_counter, li_frequency;
	LONG nb_responses;
	int timer_index;

	// Init - find out if we have access to a monotonic (hires) timer
	if (!QueryPerformanceFrequency(&li_frequency)) {
		usbi_dbg("no hires timer available on this platform");
		hires_frequency = 0;
		hires_ticks_to_ps = UINT64_C(0);
	} else {
		hires_frequency = li_frequency.QuadPart;
		// The hires frequency can go as high as 4 GHz, so we'll use a conversion
		// to picoseconds to compute the tv_nsecs part in clock_gettime
		hires_ticks_to_ps = UINT64_C(1000000000000) / hires_frequency;
		usbi_dbg("hires timer available (Frequency: %"PRIu64" Hz)", hires_frequency);
	}

	// Main loop - wait for requests
	while (1) {
		timer_index = WaitForMultipleObjects(2, timer_request, FALSE, INFINITE) - WAIT_OBJECT_0;
		if ( (timer_index != 0) && (timer_index != 1) ) {
			usbi_dbg("failure to wait on requests: %s", windows_error_str(0));
			continue;
		}
		if (request_count[timer_index] == 0) {
			// Request already handled
			ResetEvent(timer_request[timer_index]);
			// There's still a possiblity that a thread sends a request between the
			// time we test request_count[] == 0 and we reset the event, in which case
			// the request would be ignored. The simple solution to that is to test
			// request_count again and process requests if non zero.
			if (request_count[timer_index] == 0)
				continue;
		}
		switch (timer_index) {
		case 0:
			WaitForSingleObject(timer_mutex, INFINITE);
			// Requests to this thread are for hires always
			if (QueryPerformanceCounter(&hires_counter) != 0) {
				timer_tp.tv_sec = (long)(hires_counter.QuadPart / hires_frequency);
				timer_tp.tv_nsec = (long)(((hires_counter.QuadPart % hires_frequency)/1000) * hires_ticks_to_ps);
			} else {
				// Fallback to real-time if we can't get monotonic value
				// Note that real-time clock does not wait on the mutex or this thread.
				wince_clock_gettime(USBI_CLOCK_REALTIME, &timer_tp);
			}
			ReleaseMutex(timer_mutex);

			nb_responses = InterlockedExchange((LONG*)&request_count[0], 0);
			if ( (nb_responses)
			  && (ReleaseSemaphore(timer_response, nb_responses, NULL) == 0) ) {
				usbi_dbg("unable to release timer semaphore %d: %s", windows_error_str(0));
			}
			continue;
		case 1: // time to quit
			usbi_dbg("timer thread quitting");
			return 0;
		}
	}
	usbi_dbg("ERROR: broken timer thread");
	return 1;
}

static int wince_clock_gettime(int clk_id, struct timespec *tp)
{
	FILETIME filetime;
	ULARGE_INTEGER rtime;
	DWORD r;
	SYSTEMTIME st;
	switch(clk_id) {
	case USBI_CLOCK_MONOTONIC:
		if (hires_frequency != 0) {
			while (1) {
				InterlockedIncrement((LONG*)&request_count[0]);
				SetEvent(timer_request[0]);
				r = WaitForSingleObject(timer_response, TIMER_REQUEST_RETRY_MS);
				switch(r) {
				case WAIT_OBJECT_0:
					WaitForSingleObject(timer_mutex, INFINITE);
					*tp = timer_tp;
					ReleaseMutex(timer_mutex);
					return LIBUSB_SUCCESS;
				case WAIT_TIMEOUT:
					usbi_dbg("could not obtain a timer value within reasonable timeframe - too much load?");
					break; // Retry until successful
				default:
					usbi_dbg("WaitForSingleObject failed: %s", windows_error_str(0));
					return LIBUSB_ERROR_OTHER;
				}
			}
		}
		// Fall through and return real-time if monotonic was not detected @ timer init
	case USBI_CLOCK_REALTIME:
		// We follow http://msdn.microsoft.com/en-us/library/ms724928%28VS.85%29.aspx
		// with a predef epoch_time to have an epoch that starts at 1970.01.01 00:00
		// Note however that our resolution is bounded by the Windows system time
		// functions and is at best of the order of 1 ms (or, usually, worse)
		GetSystemTime(&st);
		SystemTimeToFileTime(&st, &filetime);
		rtime.LowPart = filetime.dwLowDateTime;
		rtime.HighPart = filetime.dwHighDateTime;
		rtime.QuadPart -= epoch_time;
		tp->tv_sec = (long)(rtime.QuadPart / 10000000);
		tp->tv_nsec = (long)((rtime.QuadPart % 10000000)*100);
		return LIBUSB_SUCCESS;
	default:
		return LIBUSB_ERROR_INVALID_PARAM;
	}
}

const struct usbi_os_backend wince_backend = {
        "Windows CE",
        0,
        wince_init,
        wince_exit,

        wince_get_device_list,
	NULL,				/* hotplug_poll */
        wince_open,
        wince_close,

        wince_get_device_descriptor,
        wince_get_active_config_descriptor,
        wince_get_config_descriptor,
	NULL,				/* get_config_descriptor_by_value() */

        wince_get_configuration,
        wince_set_configuration,
        wince_claim_interface,
        wince_release_interface,

        wince_set_interface_altsetting,
        wince_clear_halt,
        wince_reset_device,

        wince_kernel_driver_active,
        wince_detach_kernel_driver,
        wince_attach_kernel_driver,

        wince_destroy_device,

        wince_submit_transfer,
        wince_cancel_transfer,
        wince_clear_transfer_priv,

        wince_handle_events,

        wince_clock_gettime,
        sizeof(struct wince_device_priv),
        sizeof(struct wince_device_handle_priv),
        sizeof(struct wince_transfer_priv),
        0,
};
