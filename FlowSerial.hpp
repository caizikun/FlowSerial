/*
 * \		______ _                 _____            _                      _             
 * \\		|  ___| |               |  ___|          (_)                    (_)            
 * \\\		| |_  | | _____      __ | |__ _ __   __ _ _ _ __   ___  ___ _ __ _ _ __   __ _ 
 * \\\\		|  _| | |/ _ \ \ /\ / / |  __| '_ \ / _` | | '_ \ / _ \/ _ \ '__| | '_ \ / _` |
 * \\\\\	| |   | | (_) \ V  V /  | |__| | | | (_| | | | | |  __/  __/ |  | | | | | (_| |
 * \\\\\\	\_|   |_|\___/ \_/\_/   \____/_| |_|\__, |_|_| |_|\___|\___|_|  |_|_| |_|\__, |
 * \\\\\\\	                                     __/ |                                __/ |
 * \\\\\\\\                                     |___/                                |___/ 
 * \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
 */
/** \file	FlwoSerial.hpp
 * \author		Jimmy van den Berg at Flow Engineering
 * \date		22-march-2016
 * \brief		A library for the PC to use the FlowSerial protocol.
 * \details 	FlowSerial was designed to send and receive data orderly between devices.
 * 				It is peer based so communication and behavior should be the same one both sides.
 * 				It was first designed and used for the Active Mass Damper project in 2014 by Flow Engineering.
 */

#ifndef _FLOWSERIAL_HPP_
#define _FLOWSERIAL_HPP_

#include <string>
#include <thread>
#include <semaphore.h>
#include <mutex>
#include <stdexcept>

using namespace std;

namespace FlowSerial{

	enum class State{
		idle,
		startByteReceived,
		instructionReceived,
		argumentsReceived,
		lsbChecksumReceived,
		msbChecksumReceived,
		checksumOk
	};
	enum class Instruction{
		read,
		write,
		returnRequestedData
	};
	
	/**
	 * @brief FlowSerial connection object.
	 * @details Is the portal to communicate to the other device with the FlowSerial protocol
	 */
	class BaseSocket{
	public:
		/**
		 * @brief      Constructor
		 *
		 * @param      iflowRegister    Pointer to the register.
		 * @param[in]  iregisterLength  The length of the register
		 */
		BaseSocket(uint8_t* iflowRegister, size_t iregisterLength);
		/**
		 * @brief Request data from the other FlowSerial party register.
		 * @detials When requesting more that one byte the data will be put in the input buffer lowest index first.
		 * 
		 * @param startAddress Start address of the register you want to read. If single this is the register location you will get.
		 * @param nBytes Request data from startAddress register up to nBytes.
		 */
		void sendReadRequest(uint8_t startAddress, size_t nBytes);
		/**
		 * @brief      Reads from peer address. This has a timeout functionality
		 *             of 500 ms. It will try three time before throwing an
		 *             exception
		 *
		 * @param[in]  startAddress  The start address where to begin to read
		 *                           from other peer
		 * @param      returnData    Array will be filled with requested data.
		 * @param[in]  nBytes        Number of elements that would be like to read
		 */
		virtual void read(uint8_t startAddress, uint8_t returnData[], size_t size) = 0;
		/**
		 * @brief Write to the other FlowSerial party register
		 * @details [long description]
		 * 
		 * @param startAddress Startadrres you want to fill.
		 * @param data Actual data in an array.
		 * @param arraySize Specify the array size of the data array.
		 */
		void writeToPeer(uint8_t startAddress, const uint8_t data[], size_t size);
		/**
		 * @brief Check available bytes in input buffer.
		 * @details Note that older data will be deleted when new data arrives.
		 * @return Number of byte available in input buffer.
		 */
		size_t available();
		/**
		 * @brief Copies the input buffer into dataReturn.
		 * @details It will fill up to \p FlowSertial::BaseSocket::inputAvailable bytes
		 * 
		 * @param data Array where the data will be copied.
		 */
		void getReturnedData(uint8_t dataReturn[]);
		/**
		 * @brief Sets input buffer index to zero. This way all input is cleared.
		 */
		void clearReturnedData();
		/**
		 * Own register which can be read and written to from other party.
		 * It is up to the programmer how to use this.
		 * Either actively send data to the other part or passively store data for others to read when requested.
		 */
		uint8_t* const flowRegister;
		/**
		 * Defines the size of the array where FlowSerial::BaseSocket::flowRegister points to.
		 */
		const size_t registerLength;
	protected:
		/**
		 * @brief Update function. Input the received data here in chronological order.
		 * @details The data that is been thrown in here will be handled as FlowSerial data. 
		 * After calling this function the data that was passed can be erased/overwritten.
		 * @return True when a full FlowSerial message has been received.
		 */
		bool update(const uint8_t data[], size_t arraySize);
		/**
		 * @brief Function that must be used to parse the information generated from this class to the interface.
		 * @details The data must be on chronological order. So 0 is first out.
		 * 
		 * @param data Array this must be send to the interface handling FlowSerial. 0 is first out.
		 * @param arraySize Definition for size of the array.
		 */
		virtual void sendToInterface(const uint8_t data[], size_t arraySize) = 0;
	private:
		void returnData(const uint8_t data[], size_t arraySize);
		void returnData(uint8_t data);
		void sendFlowMessage(uint8_t startAddress, const uint8_t data[], size_t arraySize, Instruction instruction);
		static constexpr uint inputBufferSize = 256;
		uint8_t inputBufferAvailable = 0;
		uint8_t inputBuffer[inputBufferSize];
		//For update function
		//Own counting checksum
		uint16_t checksum;
		//Checksum read from package
		uint16_t checksumReceived;
		//keeps track how many arguments were received
		uint argumentBytesReceived;
		//Remember what the start address was
		uint startAddress;
		//How many bytes needs reading or writing. Received from package
		uint nBytes;
		//Temporary store writing data into this buffer until the checksum is read and checked
		uint8_t flowSerialBuffer[256];
		mutex mutexInbox;
		
		State flowSerialState = State::idle;
		Instruction instruction;
	};
	/**
	 * @brief      FlowSerial implementation build for USB devices
	 * @details    It has handy connect function to connect to a Linux USB device
	 *             via /dev/ *
	 */
	class UsbSocket : public BaseSocket{
	public:
		/**
		 * @brief      Constructor for this class.
		 *
		 * @param      iflowRegister    Pointer to array that is being used to
		 *                              store received data.
		 * @param      iregisterLength  Length of array that was given.
		 */
		UsbSocket(uint8_t* iflowRegister, size_t iregisterLength);
		/**
		 * @brief      Destroys the object. Closing all open devices.
		 */
		~UsbSocket();
		/**
		 * @brief      Connects to a device. This is handy if someone wants to
		 *             switch to another interface.
		 *
		 * @param[in]  filePath  The file path
		 * @param[in]  baudRate  The baud rate
		 */
		void connectToDevice(const char filePath[], uint baudRate);
		/**
		 * @brief      Reads from peer address. This has a timeout functionality
		 *             of 500 ms. It will try three time before throwing an
		 *             exception
		 *
		 * @param[in]  startAddress  The start address where to begin to read
		 *                           from other peer
		 * @param      returnData    Array will be filled with requested data.
		 * @param[in]  nBytes        Number of elements that would be like to read
		 */
		void read(uint8_t startAddress, uint8_t returnData[], size_t size);
		/**
		 * @brief      Closes the device.
		 */
		void closeDevice();
		/**
		 * @brief      Same as FlowSerial::UsbSocket::update(0);
		 *
		 * @return     true if a message is received.
		 */
		bool update();
		/**
		 * @brief      Checks input stream for available messages. Will block if
		 *             nothing is received. It is advised to use a thread for
		 *             this.
		 *
		 * @param[in]  timeoutMs  The timeout in milliseconds
		 *
		 * @return     True is message is received
		 */
		bool update(uint timeoutMs);
		/**
		 * @brief      Starts an update thread.
		 */
		void startUpdateThread();
		/**
		 * @brief      Stops an update thread if it's running.
		 */
		void stopUpdateThread();
		/**
		 * @brief      Determines if the device is open.
		 *
		 * @return     True if open, False otherwise.
		 */
		bool is_open();
	private:
		int fd = -1;
		bool threadRunning = true;
		void updateThread();
		thread threadChild;
		sem_t producer;
		sem_t consumer;
		virtual void sendToInterface(const uint8_t data[], size_t arraySize);
	};

	class ConnectionError: public runtime_error{
	public:
		ConnectionError(string ierrorMessage = "connection error."):
			runtime_error(ierrorMessage){}
	};
	class CouldNotOpenError: public ConnectionError
	{
	public:
		CouldNotOpenError():ConnectionError("could not open device."){}
	};
	class ReadError: public ConnectionError
	{
	public:
		ReadError():ConnectionError("could not read from device."){}
	};
	class WriteError: public ConnectionError
	{
	public:
		WriteError():ConnectionError("could not write to device."){}
	};
	class TimeoutError: public ConnectionError
	{
	public:
		TimeoutError():ConnectionError("timeout reached waiting for reading of device."){}
	};
}
#endif //_FLOWSERIAL_HPP_
