/* *INDENT-OFF* */
/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_HID_IOHIDLIB_H_
#define _IOKIT_HID_IOHIDLIB_H_

#include <sys/cdefs.h>

__BEGIN_DECLS
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#include <IOKit/IOTypes.h>
#include <IOKit/IOReturn.h>

#include <IOKit/hid/IOHIDKeys.h>

struct IOHIDEventStruct
{
    IOHIDElementType	type;
    IOHIDElementCookie	elementCookie;
    SInt32		value;
    AbsoluteTime	timestamp;
    UInt32		longValueSize;
    void *		longValue;
};
typedef struct IOHIDEventStruct IOHIDEventStruct;

/* FA12FA38-6F1A-11D4-BA0C-0005028F18D5 */
#define kIOHIDDeviceUserClientTypeID CFUUIDGetConstantUUIDWithBytes(NULL, \
    0xFA, 0x12, 0xFA, 0x38, 0x6F, 0x1A, 0x11, 0xD4,			\
    0xBA, 0x0C, 0x00, 0x05, 0x02, 0x8F, 0x18, 0xD5)

/* 13AA9C44-6F1B-11D4-907C-0005028F18D5 */
#define kIOHIDDeviceFactoryID CFUUIDGetConstantUUIDWithBytes(NULL,	\
    0x13, 0xAA, 0x9C, 0x44, 0x6F, 0x1B, 0x11, 0xD4,			\
    0x90, 0x7C, 0x00, 0x05, 0x02, 0x8F, 0x18, 0xD5)

/* 78BD420C-6F14-11D4-9474-0005028F18D5 */
/*! @defined kIOHIDDeviceInterfaceID
    @discussion Interface ID for the IOHIDDeviceInterface. Corresponds to an
                 available HID device. */
#define kIOHIDDeviceInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL, 	\
    0x78, 0xBD, 0x42, 0x0C, 0x6F, 0x14, 0x11, 0xD4,			\
    0x94, 0x74, 0x00, 0x05, 0x02, 0x8F, 0x18, 0xD5)
    
/* 7D0B510E-16D5-11D7-9E9B-000393992E38 */
/*! @defined kIOHIDDeviceInterfaceID121
    @discussion Interface ID for the IOHIDDeviceInterface121. Corresponds to 
                an available HID device that includes methods from
                IOHIDDeviceInterface.  This interface is available on 
                IOHIDLib 1.2.1 and Mac OS X 10.2.3 or later.*/
#define kIOHIDDeviceInterfaceID121 CFUUIDGetConstantUUIDWithBytes(NULL, \
    0x7d, 0xb, 0x51, 0xe, 0x16, 0xd5, 0x11, 0xd7, 			\
    0x9e, 0x9b, 0x0, 0x3, 0x93, 0x99, 0x2e, 0x38)

/* B70ABF31-16D5-11D7-AB35-000393992E38 */
/*! @defined kIOHIDDeviceInterfaceID122
    @discussion Interface ID for the IOHIDDeviceInterface122. Corresponds to 
                an available HID device that includes methods from
                IOHIDDeviceInterface and IOHIDDeviceInterface121. This 
                interface is available on IOHIDLib 1.2.2 and Mac OS X 10.3
                or later.*/
#define kIOHIDDeviceInterfaceID122 CFUUIDGetConstantUUIDWithBytes(NULL, \
    0xb7, 0xa, 0xbf, 0x31, 0x16, 0xd5, 0x11, 0xd7, 			\
    0xab, 0x35, 0x0, 0x3, 0x93, 0x99, 0x2e, 0x38)

/* 8138629E-6F14-11D4-970E-0005028F18D5 */
/*! @defined kIOHIDQueueInterfaceID
    @discussion Interface ID for the kIOHIDQueueInterfaceID. Corresponds to a
                queue for a specific HID device. */
#define kIOHIDQueueInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL, 	\
    0x81, 0x38, 0x62, 0x9E, 0x6F, 0x14, 0x11, 0xD4,			\
    0x97, 0x0E, 0x00, 0x05, 0x02, 0x8F, 0x18, 0xD5)

/* 80CDCC00-755D-11D4-8E0F-0005028F18D5 */
/*! @defined kIOHIDOutputTransactionInterfaceID
    @discussion Interface ID for the kIOHIDOutputTransactionInterfaceID.
                Corresponds to an output transaction for one or more report IDs
                on a specific device. */
#define kIOHIDOutputTransactionInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL,\
    0x80, 0xCD, 0xCC, 0x00, 0x75, 0x5D, 0x11, 0xD4,			\
    0x80, 0xEF, 0x00, 0x05, 0x02, 0x8F, 0x18, 0xD5)

/*! @typedef IOHIDCallbackFunction
    @discussion Type and arguments of callout C function that is used when a
                completion routine is called, see
                IOHIDLib.h:setRemovalCallback().
    @param target void * pointer to your data, often a pointer to an object.
    @param result Completion result of desired operation.
    @param refcon void * pointer to more data.
    @param sender Interface instance sending the completion routine.
*/
typedef void (*IOHIDCallbackFunction)
              (void * target, IOReturn result, void * refcon, void * sender);

/*! @typedef IOHIDElementCallbackFunction
    @discussion Type and arguments of callout C function that is used when a
                completion routine is called, see IOHIDLib.h:setElementValue().
    @param target void * pointer to your data, often a pointer to an object.
    @param result Completion result of desired operation.
    @param refcon void * pointer to more data.
    @param sender Interface instance sending the completion routine.
    @param elementCookie Element within interface instance sending completion.
*/
typedef void (*IOHIDElementCallbackFunction)
              (void *	 		target,
               IOReturn 		result,
               void * 			refcon,
               void * 			sender,
               IOHIDElementCookie 	elementCookie);

/*! @typedef IOHIDReportCallbackFunction
    @discussion Type and arguments of callout C function that is used when a
                completion routine is called, see IOHIDLib.h:setReport().
    @param target void * pointer to your data, often a pointer to an object.
    @param result Completion result of desired operation.
    @param refcon void * pointer to more data.
    @param sender Interface instance sending the completion routine.
    @param bufferSize Size of the buffer received upon completion.
*/
typedef void (*IOHIDReportCallbackFunction)
              (void *	 		target,
               IOReturn 		result,
               void * 			refcon,
               void * 			sender,
               UInt32		 	bufferSize);
               

/* Forward declarations of the queue and output transaction interfaces */
struct IOHIDQueueInterface;
struct IOHIDOutputTransactionInterface;
typedef struct IOHIDQueueInterface IOHIDQueueInterface;
typedef struct IOHIDOutputTransactionInterface IOHIDOutputTransactionInterface;

//
// IOHIDDeviceInterface Functions available in version 1.0 (10.0) and higher of Mac OS X
//
#define IOHIDDEVICEINTERFACE_FUNCS_100 							\
    IOReturn (*createAsyncEventSource)(void * self, CFRunLoopSourceRef * source); 	\
    CFRunLoopSourceRef (*getAsyncEventSource)(void * self);				\
    IOReturn (*createAsyncPort)(void * self, mach_port_t * port);			\
    mach_port_t (*getAsyncPort)(void * self);						\
    IOReturn (*open)(void * self, UInt32 flags);					\
    IOReturn (*close)(void * self);							\
    IOReturn (*setRemovalCallback)(void * self, IOHIDCallbackFunction removalCallback,	\
                                   void * removalTarget, void * removalRefcon);		\
    IOReturn (*getElementValue)(void * self, IOHIDElementCookie	elementCookie,		\
                                IOHIDEventStruct * valueEvent);				\
    IOReturn (*setElementValue)(void * self, IOHIDElementCookie elementCookie,		\
                                IOHIDEventStruct * valueEvent, UInt32 timeoutMS,	\
                                IOHIDElementCallbackFunction callback,			\
                                void * callbackTarget, void * callbackRefcon);		\
    IOReturn (*queryElementValue)(void * self, IOHIDElementCookie elementCookie,	\
                                IOHIDEventStruct * valueEvent, UInt32 timeoutMS,	\
                                IOHIDElementCallbackFunction callback,			\
                                void * callbackTarget, void * callbackRefcon);		\
    IOReturn (*startAllQueues)(void * self);						\
    IOReturn (*stopAllQueues)(void * self);						\
    IOHIDQueueInterface ** (*allocQueue) (void *self);					\
    IOHIDOutputTransactionInterface ** (*allocOutputTransaction) (void *self)
    
//
// IOHIDDeviceInterface Functions available in version 1.2.1 (10.2.3) and higher of Mac OS X
//
#define IOHIDDEVICEINTERFACE_FUNCS_121	 						\
    IOReturn (*setReport)(void * self, IOHIDReportType reportType, UInt32 reportID,	\
                                void * reportBuffer, UInt32 reportBufferSize,		\
                                UInt32 timeoutMS, IOHIDReportCallbackFunction callback,	\
                                void * callbackTarget, void * callbackRefcon);		\
    IOReturn (*getReport)(void * self, IOHIDReportType reportType,			\
                                UInt32 reportID, void * reportBuffer,			\
                                UInt32 * reportBufferSize, UInt32 timeoutMS,		\
                                IOHIDReportCallbackFunction callback,			\
                                void * callbackTarget, void * callbackRefcon)
                                
//
// IOHIDDeviceInterface Functions available in version 1.2.2 (10.3) and higher of Mac OS X
//
#define IOHIDDEVICEINTERFACE_FUNCS_122	 						\
    IOReturn (*copyMatchingElements)(void * self, CFDictionaryRef matchingDict, 	\
                                CFArrayRef * elements);					\
    IOReturn (*setInterruptReportHandlerCallback)(void * self, void * reportBuffer, 		\
                                UInt32 reportBufferSize, 				\
                                IOHIDReportCallbackFunction callback,			\
                                void * callbackTarget, void * callbackRefcon)	

typedef struct IOHIDDeviceInterface
{
    IUNKNOWN_C_GUTS;
    IOHIDDEVICEINTERFACE_FUNCS_100;
    IOHIDDEVICEINTERFACE_FUNCS_121;
} IOHIDDeviceInterface;

typedef struct IOHIDDeviceInterface121
{
    IUNKNOWN_C_GUTS;
    IOHIDDEVICEINTERFACE_FUNCS_100;
    IOHIDDEVICEINTERFACE_FUNCS_121;
} IOHIDDeviceInterface121;

typedef struct IOHIDDeviceInterface122
{
    IUNKNOWN_C_GUTS;
    IOHIDDEVICEINTERFACE_FUNCS_100;
    IOHIDDEVICEINTERFACE_FUNCS_121;
    IOHIDDEVICEINTERFACE_FUNCS_122;
} IOHIDDeviceInterface122;


//
// IOHIDQueueInterface Functions available in version 1.0 (10.0) and higher of Mac OS X
//
#define IOHIDQUEUEINTERFACE_FUNCS_100							\
    IOReturn (*createAsyncEventSource)(void * self, CFRunLoopSourceRef * source);	\
    CFRunLoopSourceRef (*getAsyncEventSource)(void * self);				\
    IOReturn (*createAsyncPort)(void * self, mach_port_t * port);			\
    mach_port_t (*getAsyncPort)(void * self);						\
    IOReturn (*create)(void * self, UInt32 flags, UInt32 depth);			\
    IOReturn (*dispose)(void * self);							\
    IOReturn (*addElement)(void * self, IOHIDElementCookie elementCookie, UInt32 flags);\
    IOReturn (*removeElement)(void * self, IOHIDElementCookie elementCookie);		\
    Boolean (*hasElement)(void * self, IOHIDElementCookie elementCookie);		\
    IOReturn (*start)(void * self);							\
    IOReturn (*stop)(void * self);							\
    IOReturn (*getNextEvent)(void * self, IOHIDEventStruct * event,			\
                                AbsoluteTime maxTime, UInt32 timeoutMS);		\
    IOReturn (*setEventCallout)(void * self, IOHIDCallbackFunction callback,		\
                                void * callbackTarget,  void * callbackRefcon);		\
    IOReturn (*getEventCallout)(void * self, IOHIDCallbackFunction * outCallback,	\
                                void ** outCallbackTarget, void ** outCallbackRefcon)

struct IOHIDQueueInterface
{
    IUNKNOWN_C_GUTS;
    IOHIDQUEUEINTERFACE_FUNCS_100;
};

//
// IOHIDOutputTransactionInterface Functions available in version 1.2 (10.2) and higher of Mac OS X
//
#define IOHIDOUTPUTTRANSACTIONINTERFACE_FUNCS_120 					\
    IOReturn (*createAsyncEventSource)(void * self, CFRunLoopSourceRef * source);	\
    CFRunLoopSourceRef (*getAsyncEventSource)(void * self);				\
    IOReturn (*createAsyncPort)(void * self, mach_port_t * port);			\
    mach_port_t (*getAsyncPort)(void * self);						\
    IOReturn (*create)(void * self);							\
    IOReturn (*dispose)(void * self);							\
    IOReturn (*addElement)(void * self, IOHIDElementCookie elementCookie);		\
    IOReturn (*removeElement)(void * self, IOHIDElementCookie elementCookie);		\
    Boolean  (*hasElement)(void * self, IOHIDElementCookie elementCookie);		\
    IOReturn (*setElementDefault)(void *self, IOHIDElementCookie elementCookie,		\
                                IOHIDEventStruct * valueEvent);				\
    IOReturn (*getElementDefault)(void * self, IOHIDElementCookie elementCookie,	\
                                IOHIDEventStruct * outValueEvent);			\
    IOReturn (*setElementValue)(void * self, IOHIDElementCookie elementCookie,		\
                                IOHIDEventStruct * valueEvent);				\
    IOReturn (*getElementValue)(void * self, IOHIDElementCookie elementCookie,		\
                                IOHIDEventStruct * outValueEvent);			\
    IOReturn (*commit)(void * self, UInt32 timeoutMS, IOHIDCallbackFunction callback,	\
                                void * callbackTarget, void * callbackRefcon);		\
    IOReturn (*clear)(void * self)

struct IOHIDOutputTransactionInterface
{
    IUNKNOWN_C_GUTS;
    IOHIDOUTPUTTRANSACTIONINTERFACE_FUNCS_120;
};


//
//  BEGIN READABLE STRUCTURE DEFINITIONS 
//  
//  This portion of uncompiled code provides a more reader friendly representation of 
//  the CFPlugin methods defined above.

#if 0
/*! @class IOHIDDeviceInterface
    @discussion CFPlugin object subclass which provides the primary interface to
                HID devices.
*/
typedef struct IOHIDDeviceInterface
{

    IUNKNOWN_C_GUTS;

/*! @function createAsyncEventSource
    @abstract Creates async eventsource.
    @discussion This method will create an async mach port, if one
        has not already been created.
    @param source Reference to CFRunLoopSourceRef that is created.
    @result Returns an IOReturn code.
*/
    IOReturn (*createAsyncEventSource)(void * 			self,
                                       CFRunLoopSourceRef * 	source);

/*! @function getAsyncEventSource
    @abstract Gets the created async event source.
    @result Returns a CFRunLoopSourceRef.
*/
    CFRunLoopSourceRef (*getAsyncEventSource)(void * self);
    
/*! @function createAsyncPort
    @abstract Creates an async port.
    @discussion The port must be created before any callbacks can be used. 
    @param port Reference to mach port that is created. 
    @result Returns an IOReturn code.
*/
    IOReturn (*createAsyncPort)(void * self, mach_port_t * port);
    
/*! @function getAsyncPort
    @abstract Gets the current async port.
    @result Returns a mach_port_t.
*/
    mach_port_t (*getAsyncPort)(void * self);
    
/*! @function open
    @abstract Opens the device.
    @param flags Flags to be passed down to the user client.
    @result Returns an IOReturn code.
*/
    IOReturn (*open)(void * self, UInt32 flags);
    
/*! @function close
    @abstract Closes the device.
    @result Returns an IOReturn code.
*/
    IOReturn (*close)(void * self);

/*! @function setRemovalCallback
    @abstract Sets callback to be used when device is removed.
    @param removalCallback Called when the device is removed. 
    @param removeTarget Passed to the callback.
    @param removalRefcon Passed to the callback.
    @result Returns an IOReturn code.
*/
    IOReturn (*setRemovalCallback)(void * 			self,
                                   IOHIDCallbackFunction	removalCallback,
                                   void *			removalTarget,
                                   void *			removalRefcon);

/*! @function getElementValue
    @abstract Obtains the most recent value of an element.
    @discussion This call is most useful for interrupt driven elements,
        such as input type elements.  Since feature type element values 
        need to be polled from the device, it is recommended to use the 
        queryElementValue method to obtain the current value.  The  
        timestamp field in the event details the last time the element 
        value was altered.
    @param elementCookie The element of interest. 
    @param valueEvent The event that will be filled.   If a long value is 
        present, it is up to the caller to deallocate it.
    @result Returns an IOReturn code.
*/
    IOReturn (*getElementValue)(void * 			self,
                                IOHIDElementCookie	elementCookie,
                                IOHIDEventStruct *	valueEvent);

/*! @function setElementValue
    @abstract Sets an element value on the device.
    @discussion This call is most useful for feature type elements.  It is
        recommended to use IOOutputTransaction for output type elements.
    @param elementCookie The element of interest. 
    @param valueEvent The event that will be filled.  If a long value is
        present, it will be copied.
    @param timeoutMS UNSUPPORTED.
    @param callback UNSUPPORTED.
    @param callbackTarget UNSUPPORTED.
    @param callbackRefcon UNSUPPORTED.
    @result Returns an IOReturn code.
*/
    IOReturn (*setElementValue)(void *	 			self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

/*! @function queryElementValue
    @abstract Obtains the current value of an element.
    @discussion This call is most useful for feature type elements.  This
        method will poll the device for the current element value.
    @param elementCookie The element of interest. 
    @param valueEvent The event that will be filled.  If a long value is 
        present, it is up to the caller to deallocate it.
    @param timeoutMS UNSUPPORTED.
    @param callback UNSUPPORTED.
    @param callbackTarget UNSUPPORTED.
    @param callbackRefcon UNSUPPORTED.
    @result Returns an IOReturn code.
*/
    IOReturn (*queryElementValue)(void * 			self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

/*! @function startAllQueues
    @abstract Starts data delivery on all queues for this device.
    @result Returns an IOReturn code.
*/
    IOReturn (*startAllQueues)(void * self);
    
/*! @function stopAllQueues
    @abstract Stops data delivery on all queues for this device.
    @result Returns an IOReturn code.
*/
    IOReturn (*stopAllQueues)(void * self);

/*! @function allocQueue
    @abstract Wrapper to return instances of the IOHIDQueueInterface. 
    @result Returns the created IOHIDQueueInterface.
*/
    IOHIDQueueInterface ** (*allocQueue) (void *self);
    
/*! @function allocOutputTransaction
    @abstract Wrapper to return instances of the IOHIDOutputTransactionInterface. 
    @result Returns the created IOHIDOutputTransactionInterface.
*/
    IOHIDOutputTransactionInterface ** (*allocOutputTransaction) (void *self);
    
} IOHIDDeviceInterface;

/*! @class IOHIDDeviceInterface121
    @discussion CFPlugin object subclass which provides the primary interface to
                HID devices.  This class is a subclass of IOHIDDeviceInterface.
*/
typedef struct IOHIDDeviceInterface121
{

    IUNKNOWN_C_GUTS;
    IOHIDDEVICEINTERFACE_FUNCS_100;

/*! @function setReport
    @abstract Sends a report to the device.
    @param reportType The report type.
    @param reportID The report id.
    @param reportBuffer Pointer to a preallocated buffer.
    @param reportBufferSize Size of the reportBuffer in bytes.
    @param timeoutMS
    @param callback If null, this method will behave synchronously.
    @param callbackTarget The callback target passed to the callback.
    @param callbackRefcon The callback refcon passed to the callback. 
    @result Returns an IOReturn code.
*/
    IOReturn (*setReport)	(void * 			self,
                                IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);

/*! @function getReport
    @abstract Obtains a report from the device.
    @param reportType The report type.
    @param reportID The report ID.
    @param reportBuffer Pointer to a preallocated buffer.
    @param reportBufferSize Size of the reportBuffer in bytes.  
        When finished, will contain the actual size of the report.
    @param timeoutMS
    @param callback If null, this method will behave synchronously.
    @param callbackTarget The callback target passed to the callback.
    @param callbackRefcon The callback refcon passed to the callback. 
    @result Returns an IOReturn code.
*/
    IOReturn (*getReport)	(void * 			self,
                                IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32 *			reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon);
                                
}IOHIDDeviceInterface121;

/*! @class IOHIDDeviceInterface122
    @discussion CFPlugin object subclass which provides the primary interface to
                HID devices.  This class is a subclass of IOHIDDeviceInterface121.
*/
typedef struct IOHIDDeviceInterface122
{

    IUNKNOWN_C_GUTS;
    IOHIDDEVICEINTERFACE_FUNCS_100;
    IOHIDDEVICEINTERFACE_FUNCS_121;

/*! @function copyMatchingElements
    @abstract Obtains specific elements defined by the device.
    @discussion Using keys defined in IOHIDKeys.h for elements, create a 
            matching dictonary containing items that you wish to search for.  
            A null array indicates that no elements matching that criteria 
            were found. Each item in the array is a reference to the same 
            dictionary item that represents each element in the I/O Registry.
            It is up to the caller to release the returned array of elements. 
    @param matchingDict Dictionary containg key/value pairs to match on.  Pass
            a null value to match on all elements.
    @param elements Pointer to a CFArrayRef that will be returned by this
            method.  It is up to the caller to release it when finished. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*copyMatchingElements)(void * 			self, 
                                CFDictionaryRef 		matchingDict, 
                                CFArrayRef * 			elements);
                                
/*! @function setInterruptReportHandlerCallback
    @abstract Sets the report handler callout to be called when the data 
        is received from the Interrupt-In pipe.
    @discussion In order for this to work correctly, you must call
        createAsyncPort and createAsyncEventSource.
    @param reportBuffer Pointer to a preallocated buffer.
    @param reportBufferSize Size of the reportBuffer in bytes.  
    @param callback If non-NULL, is a callback to be called when data 
        is received from the device.
    @param callbackTarget The callback target passed to the callback
    @param callbackRefcon The callback refcon passed to the callback.
    @result Returns an IOReturn code. 
*/
    IOReturn (*setInterruptReportHandlerCallback)(
                            void * 				self,
                            void *				reportBuffer,
                            UInt32				reportBufferSize, 
                            IOHIDReportCallbackFunction 	callback,
                            void * 				callbackTarget, 
                            void * 				callbackRefcon);
    
}IOHIDDeviceInterface122;

/*! @class IOHIDQueueInterface
    @discussion CFPlugin object subclass which provides an interface for input
                queues from HID devices. Created by an IOHIDDeviceInterface
                object.
*/
typedef struct IOHIDQueueInterface
{

    IUNKNOWN_C_GUTS;

/*! @function createAsyncEventSource
    @abstract Creates an async event source.
    @discussion This will be used with setEventCallout.
    @param source The newly created event source.
    @result Returns an IOReturn code. 
*/
    IOReturn (*createAsyncEventSource)(void * 			self, 
                                        CFRunLoopSourceRef * 	source);

/*! @function getAsyncEventSource
    @abstract Obtains the current event source.
    @result Returns a CFRunLoopSourceRef.
*/
    CFRunLoopSourceRef (*getAsyncEventSource)(void * self);

/*! @function createAsyncPort
    @abstract Creates an async port.
    @discussion This will be used with createAsyncEventSource.
    @param port The newly created async port.
    @result Returns an IOReturn code.
*/
    IOReturn (*createAsyncPort)(void * self, mach_port_t * port);
    
/*! @function getAsyncPort
    @abstract Obtains the current async port.
    @result Returns a mach_port_t.
*/
    mach_port_t (*getAsyncPort)(void * self);
    
/*! @function create
    @abstract Creates the current queue. 
    @param flags
    @param depth The maximum number of elements in the queue 
        before the oldest elements in the queue begin to be lost.
    @result Returns an IOReturn code. 
*/
    IOReturn (*create)(void * 			self, 
                        UInt32 			flags,
                        UInt32			depth);

/*! @function create
    @abstract Disposes of the current queue. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*dispose)(void * self);
    
/*! @function addElement
    @abstract Adds an element to the queue.
    @discussion If the element has already been added to queue,
        an error will be returned.
    @param elementCookie The element of interest. 
    @param flags 
    @result Returns an IOReturn code. 
*/
    IOReturn (*addElement)(void * self,
                           IOHIDElementCookie elementCookie,
                           UInt32 flags);

/*! @function removeElement
    @abstract Removes an element from the queue.
    @discussion If the element has not been added to queue,
        an error will be returned.
    @param elementCookie The element of interest. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*removeElement)(void * self, IOHIDElementCookie elementCookie);
    
/*! @function hasElement
    @abstract Checks whether an element has been added to 
        the queue.
    @discussion Will return true if present, otherwise will return false.
    @param elementCookie The element of interest. 
    @result Returns a Boolean value. 
*/
    Boolean (*hasElement)(void * self, IOHIDElementCookie elementCookie);

/*! @function start
    @abstract Starts event delivery to the queue. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*start)(void * self);
    
/*! @function stop
    @abstract Stops event delivery to the queue. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*stop)(void * self);

/*! @function getNextEvent
    @abstract Reads next event from the queue.
    @param event The event that will be filled.  If a long value is
        present, it is up to the caller to deallocate it.
    @param maxtime UNSUPPORTED.  If non-zero, limits read events to 
        those that occured on or before maxTime.
    @param timoutMS UNSUPPORTED.  The timeout in milliseconds, a zero  
        timeout will cause this call to be non-blocking (returning  
        queue empty) if there is a NULL callback, and blocking forever 
        until the queue is non-empty if there is a valid callback.
    @result Returns an IOReturn code. 
*/
    IOReturn (*getNextEvent)(void * 			self,
                            IOHIDEventStruct *		event,
                            AbsoluteTime		maxTime,
                            UInt32 			timeoutMS);

/*! @function setEventCallout
    @abstract Sets the event callout to be called when the queue 
        transitions to non-empty.
    @discussion In order for this to work correctly, you must call
        createAsyncPort and createAsyncEventSource.
    @param callback if non-NULL is a callback to be called when data 
        is  inserted to the queue
    @param callbackTarget The callback target passed to the callback
    @param callbackRefcon The callback refcon passed to the callback.
    @result Returns an IOReturn code.
*/
    IOReturn (*setEventCallout)(void * 			self,
                                IOHIDCallbackFunction   callback,
                                void * 			callbackTarget,
                                void *			callbackRefcon);

/*! @function getEventCallout
    @abstract Gets the event callout.
    @discussion This callback will be called the queue transitions
        to non-empty.
    @param callback if non-NULL is a callback to be called when data 
        is  inserted to the queue
    @param callbackTarget The callback target passed to the callback
    @param callbackRefcon The callback refcon passed to the callback 
    @result Returns an IOReturn code. 
*/
    IOReturn (*getEventCallout)(void * 			self,
                                IOHIDCallbackFunction * outCallback,
                                void ** 		outCallbackTarget,
                                void **			outCallbackRefcon);
} IOHIDQueueInterface;

/*! @class IOHIDOutputTransactionInterface
    @discussion CFPlugin object subclass which privides interface for output
                transactions to HID devices. Created by a IOHIDDeviceInterface
                object. */

typedef struct IOHIDOutputTransactionInterface
{
    IUNKNOWN_C_GUTS;
                    
/*! @function createAsyncEventSource
    @abstract Creates an async event source.
    @discussion This will be used with setEventCallout.
    @param source The newly created event source 
    @result Returns an IOReturn code. 
*/
    IOReturn (*createAsyncEventSource)(void * 			self, 
                                        CFRunLoopSourceRef * 	source);

/*! @function getAsyncEventSource
    @abstract Obtains the current event source.
    @result Returns a CFRunLoopSourceRef.
*/
    CFRunLoopSourceRef (*getAsyncEventSource)(void * self);

/*! @function createAsyncPort
    @abstract Creates an async port.
    @discussion This will be used with createAsyncEventSource.
    @param port The newly created async port. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*createAsyncPort)(void * self, mach_port_t * port);
    
/*! @function getAsyncPort
    @abstract Obtains the current async port. 
    @result Returns a mach_port_t.
*/
    mach_port_t (*getAsyncPort)(void * self);
    
/*! @function create
    @abstract Creates the current transaction.
    @discussion This method will free any memory that has been
        allocated for this transaction. 
    @result Returns an IOReturn code.
*/
    IOReturn (*create)(void * self);
    
/*! @function dispose
    @abstract Disposes of the current transaction.
    @discussion The transaction will have to be recreated, in order
        to perform any operations on the transaction. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*dispose)(void * self);
    
/*! @function addElement
    @abstract Adds an element to the transaction.
    @discussion If the element has already been added to transaction,
        an error will be returned.
    @param elementCookie The element of interest.
    @result Returns an IOReturn code. 
*/
    IOReturn (*addElement)	(void * self, IOHIDElementCookie elementCookie);
    
/*! @function removeElement
    @abstract Removes an element from the transaction.
    @discussion If the element has not been added to transaction,
        an error will be returned.
    @param elementCookie The element of interest. 
    @result Returns an IOReturn code.
*/
    IOReturn (*removeElement)	(void * self, IOHIDElementCookie elementCookie);
    
/*! @function hasElement
    @abstract Checks whether an element has been added to 
        the transaction.
    @discussion Will return true if present, otherwise will return false.
    @param elementCookie The element of interest. 
    @result Returns a Boolean value. 
*/
    Boolean  (*hasElement)	(void * self, IOHIDElementCookie elementCookie);
    
/*! @function setElementDefault
    @abstract Sets the default value of an element in a 
        transaction.
    @discussion An error will be returned if the element has not been
        added to the transaction.
    @param elementCookie The element of interest. 
    @param valueEvent The event that will be filled.  If a long value is
        present, it will be copied. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*setElementDefault)(void *	 	self,
                                    IOHIDElementCookie	elementCookie,
                                    IOHIDEventStruct *	valueEvent);
    
/*! @function getElementDefault
    @abstract Obtains the default value of an element in a 
        transaction.
    @discussion An error will be returned if the element has not been 
        added to the transaction.
    @param elementCookie The element of interest. 
    @param outValueEvent The event that will be filled.  If a long value is 
        present, it is up to the caller to deallocate it. 
    @result Returns an IOReturn code.
*/
    IOReturn (*getElementDefault)(void *	 	self,
                                    IOHIDElementCookie	elementCookie,
                                    IOHIDEventStruct *	outValueEvent);
    
/*! @function setElementValue
    @abstract Sets the value of an element in a transaction.
    @discussion An error will be returned if the element has not been
        added to the transaction.
    @param elementCookie The element of interest. 
    @param valueEvent The event that will be filled.  If a long value is
        present, it will be copied.
    @result Returns an IOReturn code. 
*/
    IOReturn (*setElementValue)(void *	 		self,
                                IOHIDElementCookie	elementCookie,
                                IOHIDEventStruct *	valueEvent);
    
/*! @function getElementValue
    @abstract Obtains the value of an element in a transaction.
    @discussion An error will be returned if the element has not been 
        added to the transaction.
    @param elementCookie The element of interest. 
    @param outValueEvent The event that will be filled.  If a long value is 
        present, it is up to the caller to deallocate it. 
    @result Returns an IOReturn code.
*/
    IOReturn (*getElementValue)(void *	 		self,
                                IOHIDElementCookie	elementCookie,
                                IOHIDEventStruct *	outValueEvent);
    
/*! @function commit
    @abstract Commits the transaction.
    @discussion Transaction element values, if set, will be sent to the 
        device.  Otherwise, the default element value will be used.  If
        neither are set, that element will be omitted from the commit.
        After a transaction is committed, transaction element values 
        will be cleared.  Default values will be preserved.
    @param timeoutMS UNSUPPORTED
    @param callback UNSUPPORTED
    @param callbackTarget UNSUPPORTED
    @param callbackRefcon UNSUPPORTED 
    @result Returns an IOReturn code.
*/
    IOReturn (*commit)(void * 			self,
                        UInt32 			timeoutMS,
                        IOHIDCallbackFunction   callback,
                        void * 			callbackTarget,
                        void *			callbackRefcon);
    
/*! @function clear
    @abstract Clears the transaction.
    @discussion Transaction element values will cleared.   Default 
        values will be preserved. 
    @result Returns an IOReturn code. 
*/
    IOReturn (*clear)(void * self);
} IOHIDOutputTransactionInterface;

#endif

__END_DECLS

#endif /* !_IOKIT_HID_IOHIDLIB_H_ */
