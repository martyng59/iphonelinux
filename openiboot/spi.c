#include "openiboot.h"
#include "openiboot-asmhelpers.h"
#include "spi.h"
#include "hardware/spi.h"
#include "util.h"
#include "clock.h"
#include "chipid.h"
#include "timer.h"
#include "interrupt.h"

static const SPIRegister SPIRegs[NUM_SPIPORTS] = {
	{SPI0 + CONTROL, SPI0 + SETUP, SPI0 + STATUS, SPI0 + UNKREG1, SPI0 + TXDATA, SPI0 + RXDATA,
		SPI0 + CLKDIVIDER, SPI0 + SPCNT, SPI0 + SPIDD, SPI0 + UNKREG4, SPI0 + UNKREG5,
		SPI0 + UNKREG6, SPI0 + UNKREG7, SPI0 + TXBUFFERLEN},
	{SPI1 + CONTROL, SPI1 + SETUP, SPI1 + STATUS, SPI1 + UNKREG1, SPI1 + TXDATA, SPI1 + RXDATA,
		SPI1 + CLKDIVIDER, SPI1 + SPCNT, SPI1 + SPIDD, SPI1 + UNKREG4, SPI1 + UNKREG5,
		SPI1 + UNKREG6, SPI1 + UNKREG7, SPI1 + TXBUFFERLEN},
	{SPI2 + CONTROL, SPI2 + SETUP, SPI2 + STATUS, SPI2 + UNKREG1, SPI2 + TXDATA, SPI2 + RXDATA,
		SPI2 + CLKDIVIDER, SPI2 + SPCNT, SPI2 + SPIDD, SPI2 + UNKREG4, SPI2 + UNKREG5,
		SPI2 + UNKREG6, SPI2 + UNKREG7, SPI2 + TXBUFFERLEN},
#ifdef CONFIG_IPOD2G
	{SPI3 + CONTROL, SPI3 + SETUP, SPI3 + STATUS, SPI3 + UNKREG1, SPI3 + TXDATA, SPI3 + RXDATA,
		SPI3 + CLKDIVIDER, SPI3 + SPCNT, SPI3 + SPIDD, SPI3 + UNKREG4, SPI3 + UNKREG5,
		SPI3 + UNKREG6, SPI3 + UNKREG7, SPI3 + TXBUFFERLEN},
	{SPI4 + CONTROL, SPI4 + SETUP, SPI4 + STATUS, SPI4 + UNKREG1, SPI4 + TXDATA, SPI4 + RXDATA,
		SPI4 + CLKDIVIDER, SPI4 + SPCNT, SPI4 + SPIDD, SPI4 + UNKREG4, SPI4 + UNKREG5,
		SPI4 + UNKREG6, SPI4 + UNKREG7, SPI4 + TXBUFFERLEN},
#endif
};

static SPIInfo spi_info[NUM_SPIPORTS];

static void spiIRQHandler(uint32_t port);

int spi_setup() {
	clock_gate_switch(SPI0_CLOCKGATE, ON);
	clock_gate_switch(SPI1_CLOCKGATE, ON);
	clock_gate_switch(SPI2_CLOCKGATE, ON);
#ifdef CONFIG_IPOD2G
	clock_gate_switch(SPI3_CLOCKGATE, ON);
	clock_gate_switch(SPI4_CLOCKGATE, ON);
#endif

	memset(spi_info, 0, sizeof(SPIInfo) * NUM_SPIPORTS);

	int i;
	for(i = 0; i < NUM_SPIPORTS; i++) {
		spi_info[i].clockSource = NCLK;
		SET_REG(SPIRegs[i].control, 0);
	}

	interrupt_install(SPI0_IRQ, spiIRQHandler, 0);
	interrupt_install(SPI1_IRQ, spiIRQHandler, 1);
	interrupt_install(SPI2_IRQ, spiIRQHandler, 2);
	interrupt_enable(SPI0_IRQ);
	interrupt_enable(SPI1_IRQ);
	interrupt_enable(SPI2_IRQ);

#ifdef CONFIG_IPOD2G
	interrupt_install(SPI3_IRQ, spiIRQHandler, 3);
	interrupt_install(SPI4_IRQ, spiIRQHandler, 4);
	interrupt_enable(SPI3_IRQ);
	interrupt_enable(SPI4_IRQ);
#endif

	return 0;
}

static void spi_txdata(int port, const volatile void *buff, int from, int to)
{
	int i=from>>spi_info[port].wordSize;
	int j=to>>spi_info[port].wordSize;
	switch(spi_info[port].wordSize)
	{
		case 0: // 8-bytes
			{
				uint8_t *buf = (uint8_t*)buff;
				for(;i<j;i++)
				{
					SET_REG(SPIRegs[port].txData, buf[i]);
				}
			}
			break;

		case 1: // 16-bytes
			{
				uint16_t *buf = (uint16_t*)buff;
				for(;i<j;i++)
				{
					SET_REG(SPIRegs[port].txData, buf[i]);
				}
			}
			break;

		case 2: // 32-bytes
			{
				uint32_t *buf = (uint32_t*)buff;
				for(;i<j;i++)
				{
					SET_REG(SPIRegs[port].txData, buf[i]);
				}
			}
			break;
	}
}

static void spi_rxdata(int port, const volatile void *buff, int from, int to)
{
	int i=from>>spi_info[port].wordSize;
	int j=to>>spi_info[port].wordSize;
	switch(spi_info[port].wordSize)
	{
		case 0: // 8-bytes
			{
				uint8_t *buf = (uint8_t*)buff;
				for(;i<j;i++)
				{
					buf[i] = GET_REG(SPIRegs[port].rxData);
				}
			}
			break;

		case 1: // 16-bytes
			{
				uint16_t *buf = (uint16_t*)buff;
				for(;i<j;i++)
				{
					buf[i] = GET_REG(SPIRegs[port].rxData);
				}
			}
			break;

		case 2: // 32-bytes
			{
				uint32_t *buf = (uint32_t*)buff;
				for(;i<j;i++)
				{
					buf[i] = GET_REG(SPIRegs[port].rxData);
				}
			}
			break;
	}
}

int spi_tx(int port, const uint8_t* buffer, int len, int block, int unknown) {
	if(port > (NUM_SPIPORTS - 1)) {
		return -1;
	}

	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 2));
	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 3));

	spi_info[port].txBuffer = buffer;
#ifdef CONFIG_IPOD2G
	spi_info[port].rxBuffer = NULL;
	spi_info[port].rxCurrentLen = 0;
	spi_info[port].rxTotalLen = 0;
#endif

	if(len > MAX_TX_BUFFER)
		spi_info[port].txCurrentLen = MAX_TX_BUFFER;
	else
		spi_info[port].txCurrentLen = len;

	spi_info[port].txTotalLen = len;
	spi_info[port].txDone = FALSE;

	if(unknown == 0) {
		SET_REG(SPIRegs[port].cnt, 0);
	} else {
#ifdef CONFIG_IPOD2G
		spi_info[port].setupOptions |= SPISETUP_UNKN1;
#endif
	}

	spi_txdata(port, buffer, 0, spi_info[port].txCurrentLen);

#ifdef CONFIG_IPOD2G
	SET_REG(SPIRegs[port].txBufferLen, len);
	spi_info[port].setupOptions |= SPISETUP_UNKN2 | SPISETUP_UNKN3;
	SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif

	SET_REG(SPIRegs[port].control, 1);

	if(block) {
		bufferPrintf("q");
		while(!spi_info[port].txDone || TX_BUFFER_LEFT(GET_REG(SPIRegs[port].status)) != 0) {
			bufferPrintf("done: %d bufferleft: %d\r\n", spi_info[port].txDone, TX_BUFFER_LEFT(GET_REG(SPIRegs[port].status)));
			// yield
		}
		bufferPrintf("w");
		return len;
	} else {
		return 0;
	}
}

int spi_rx(int port, uint8_t* buffer, int len, int block, int noTransmitJunk) {
	if(port > (NUM_SPIPORTS - 1)) {
		return -1;
	}

	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 2));
	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 3));

	spi_info[port].rxBuffer = buffer;
	spi_info[port].rxDone = FALSE;
	spi_info[port].rxCurrentLen = 0;
	spi_info[port].rxTotalLen = len;
	spi_info[port].counter = 0;
#ifdef CONFIG_IPOD2G
	spi_info[port].txBuffer = NULL;
	spi_info[port].txCurrentLen = 0;
	spi_info[port].txTotalLen = 0;
#endif

	SET_REG(SPIRegs[port].cnt, (len + ((1<<spi_info[port].wordSize)-1)) >> spi_info[port].wordSize);
	SET_REG(SPIRegs[port].control, 1);

#ifdef CONFIG_IPOD2G
	if(noTransmitJunk == 0) {
		spi_info[port].setupOptions |= 1;
	} else {
		SET_REG(SPIRegs[port].txBufferLen, len);
		spi_info[port].setupOptions |= SPISETUP_UNKN3;
	}
	spi_info[port].setupOptions |= SPISETUP_UNKN1 | SPISETUP_UNKN2;
	SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#else
	if(noTransmitJunk == 0) {
		SET_REG(SPIRegs[port].setup, GET_REG(SPIRegs[port].setup) | SPISETUP_NO_TRANSMIT_JUNK);
	}
#endif

	if(block) {
		uint64_t startTime = timer_get_system_microtime();
		while(!spi_info[port].rxDone) {
			// yield
			if(has_elapsed(startTime, 1000)) {
				EnterCriticalSection();
				spi_info[port].rxDone = TRUE;
				spi_info[port].rxBuffer = NULL;
				LeaveCriticalSection();
				if(noTransmitJunk == 0) {
#ifdef CONFIG_IPOD2G
					spi_info[port].setupOptions &= ~SPISETUP_NO_TRANSMIT_JUNK;
					SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#else
					SET_REG(SPIRegs[port].setup, GET_REG(SPIRegs[port].setup) & ~SPISETUP_NO_TRANSMIT_JUNK);
#endif
				}
				return -1;
			}
		}
		if(noTransmitJunk == 0) {
#ifdef CONFIG_IPOD2G
			spi_info[port].setupOptions &= ~SPISETUP_NO_TRANSMIT_JUNK;
			SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#else
			SET_REG(SPIRegs[port].setup, GET_REG(SPIRegs[port].setup) & ~SPISETUP_NO_TRANSMIT_JUNK);
#endif
		}
		return len;
	} else {
		return 0;
	}
}

int spi_txrx(int port, const uint8_t* outBuffer, int outLen, uint8_t* inBuffer, int inLen, int block)
{
	if(port > (NUM_SPIPORTS - 1)) {
		return -1;
	}

	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 2));
	SET_REG(SPIRegs[port].control, GET_REG(SPIRegs[port].control) | (1 << 3));

	spi_info[port].txBuffer = outBuffer;

	if(outLen > MAX_TX_BUFFER)
		spi_info[port].txCurrentLen = MAX_TX_BUFFER;
	else
		spi_info[port].txCurrentLen = outLen;

	spi_info[port].txTotalLen = outLen;
	spi_info[port].txDone = FALSE;

	spi_info[port].rxBuffer = inBuffer;
	spi_info[port].rxDone = FALSE;
	spi_info[port].rxCurrentLen = 0;
	spi_info[port].rxTotalLen = inLen;
	spi_info[port].counter = 0;

	spi_txdata(port, outBuffer, 0, spi_info[port].txCurrentLen);

	SET_REG(SPIRegs[port].cnt, (inLen + ((1<<spi_info[port].wordSize)-1)) >> spi_info[port].wordSize);
	SET_REG(SPIRegs[port].control, 1);

	if(block) {
		while(!spi_info[port].txDone || !spi_info[port].rxDone || TX_BUFFER_LEFT(GET_REG(SPIRegs[port].status)) != 0) {
			// yield
		}
		return inLen;
	} else {
		return 0;
	}
}

void spi_set_baud(int port, int baud, SPIWordSize wordSize, int isMaster, int isActiveLow, int lastClockEdgeMissing) {
	if(port > (NUM_SPIPORTS - 1)) {
		return;
	}

	SET_REG(SPIRegs[port].control, 0);

	switch(wordSize) {
		case SPIWordSize8:
			spi_info[port].wordSize = 0;
			break;

		case SPIWordSize16:
			spi_info[port].wordSize = 1;
			break;

		case SPIWordSize32:
			spi_info[port].wordSize = 2;
			break;
	}

	spi_info[port].isActiveLow = isActiveLow;
	spi_info[port].lastClockEdgeMissing = lastClockEdgeMissing;

	uint32_t clockFrequency;

	if(spi_info[port].clockSource == PCLK) {
		clockFrequency = clock_get_frequency(FrequencyBasePeripheral);
	} else {
		clockFrequency = clock_get_frequency(FrequencyBaseFixed);
	}

	uint32_t divider;

#ifdef CONFIG_IPOD2G
	divider = clockFrequency / baud;
	if (divider == 0) {
		divider = 1;
	}
#else
	if(chipid_spi_clocktype() != 0) {
		divider = clockFrequency / baud;
		if(divider < 2)
			divider = 2;
	} else {
		divider = clockFrequency / (baud * 2 - 1);
	}
#endif

	if(divider > MAX_DIVIDER) {
		return;
	}

	SET_REG(SPIRegs[port].clkDivider, divider);
	spi_info[port].baud = baud;
	spi_info[port].isMaster = isMaster;
	spi_info[port].setupOptions = (lastClockEdgeMissing << 1)
			| (isActiveLow << 2)
			| ((isMaster ? 0x3 : 0) << 3)
			| ((spi_info[port].option5 ? 0x2 : 0x1) << 5)
			| (spi_info[port].clockSource << CLOCK_SHIFT)
			| spi_info[port].wordSize << 13;
	SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#ifdef CONFIG_IPOD2G
	SET_REG(SPIRegs[port].pin, 2);
#else
	//TODO: possible bug on S5L8900? should be 2 according to iPhone disassembly
	SET_REG(SPIRegs[port].pin, 0);
#endif
	SET_REG(SPIRegs[port].control, 1);
}

void spi_process_irq_tx_rx(uint32_t port, uint32_t status, Boolean startWithTX) {
	Boolean shouldDoTX = startWithTX;
	int i;
	while (TRUE) {
		if (shouldDoTX) {
			// take care of tx
			if (spi_info[port].txBuffer != NULL) {
				if(spi_info[port].txCurrentLen < spi_info[port].txTotalLen) {
					int toTX = spi_info[port].txTotalLen - spi_info[port].txCurrentLen;
					int canTX = 8 - TX_BUFFER_LEFT(status);	//TODO: figure out why 16 != TX_BUFFER_LEFT
			
					if(toTX > canTX)
						toTX = canTX;

					for(i = 0; i < toTX; i++) {
						SET_REG(SPIRegs[port].txData, spi_info[port].txBuffer[spi_info[port].txCurrentLen + i]);
					}

					spi_info[port].txCurrentLen += toTX;
				} else {
					spi_info[port].txDone = TRUE;
					spi_info[port].txBuffer = NULL;
#ifdef CONFIG_IPOD2G
					spi_info[port].setupOptions &= ~SPISETUP_UNKN3;
					SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif
				}
			}
		}

		shouldDoTX = TRUE;

		// take care of rx
		if(spi_info[port].rxBuffer == NULL)
			break;

		int toRX = spi_info[port].rxTotalLen - spi_info[port].rxCurrentLen;
		int canRX = RX_BUFFER_LEFT(status);

		if(toRX > canRX)
			toRX = canRX;

		for(i = 0; i < toRX; i++) {
			spi_info[port].rxBuffer[spi_info[port].rxCurrentLen + i] = GET_REG(SPIRegs[port].rxData);
		}

		spi_info[port].rxCurrentLen += toRX;

		if(spi_info[port].rxCurrentLen < spi_info[port].rxTotalLen)
			break;

		spi_info[port].rxDone = TRUE;
		spi_info[port].rxBuffer = NULL;
#ifdef CONFIG_IPOD2G
		spi_info[port].setupOptions &= ~SPISETUP_UNKN1;
		SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif
	}
}

static void spiIRQHandler(uint32_t port) {
	if(port > (NUM_SPIPORTS - 1)) {
		return;
	}

	uint32_t status = GET_REG(SPIRegs[port].status);
	if(status & (1 << 3)) {
		spi_info[port].counter++;
	}

	if(status & (1 << 1)) {
		while(TRUE) {
			// take care of tx
			if(spi_info[port].txBuffer != NULL) {
				if(spi_info[port].txCurrentLen < spi_info[port].txTotalLen)
				{
					int toTX = spi_info[port].txTotalLen - spi_info[port].txCurrentLen;
					int canTX = (8 - TX_BUFFER_LEFT(status));	//TODO: figure out the 16!

					if(toTX > canTX)
						toTX = canTX;

					spi_txdata(port, spi_info[port].txBuffer, spi_info[port].txCurrentLen, spi_info[port].txCurrentLen+toTX);
					spi_info[port].txCurrentLen += toTX;

				} else {
					spi_info[port].txDone = TRUE;
					spi_info[port].txBuffer = NULL;
#ifdef CONFIG_IPOD2G
					spi_info[port].setupOptions &= ~SPISETUP_UNKN3;
					SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif
				}
			}

dorx:
			// take care of rx
			if(spi_info[port].rxBuffer == NULL)
				break;

			int toRX = spi_info[port].rxTotalLen - spi_info[port].rxCurrentLen;
			int canRX = RX_BUFFER_LEFT(status) << spi_info[port].wordSize;

			if(toRX > canRX)
				toRX = canRX;

			spi_rxdata(port, spi_info[port].rxBuffer, spi_info[port].rxCurrentLen, spi_info[port].rxCurrentLen+toRX);
			spi_info[port].rxCurrentLen += toRX;

			if(spi_info[port].rxCurrentLen < spi_info[port].rxTotalLen)
				break;

			spi_info[port].rxDone = TRUE;
			spi_info[port].rxBuffer = NULL;
#ifdef CONFIG_IPOD2G
			spi_info[port].setupOptions &= ~SPISETUP_UNKN1;
			SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif
		}


	} else  if(status & (1 << 0)) {
		// jump into middle of the loop to handle rx only, stupidly
		goto dorx;
	}

	// acknowledge interrupt handling complete
	SET_REG(SPIRegs[port].status, status);

#ifdef CONFIG_IPOD2G
	if (spi_info[port].rxBuffer == NULL && spi_info[port].txBuffer == NULL) {
		spi_info[port].setupOptions &= ~SPISETUP_UNKN2;
		SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
	}
#endif
/*
	bufferPrintf(".");
	if(port > (NUM_SPIPORTS - 1)) {
		return;
	}

	uint32_t status = GET_REG(SPIRegs[port].status);
	SET_REG(SPIRegs[port].status, status);

	if(status & (1 << 3)) {
		spi_info[port].counter++;
	}
	
	if (status & (1 << 0)) {
		if(status & ((1 << 1) | (1 << 22))) {
			spi_process_irq_tx_rx(port, status, TRUE);
		} else {
			spi_process_irq_tx_rx(port, status, FALSE);
		}
	} else {
		if (status & ((1 << 1) | (1 << 22))) {
			spi_process_irq_tx_rx(port, status, TRUE);
		}
	}

<<<<<<< Updated upstream
	if(status & (1 << 1)) {
=======
#if defined(this_is_not_defined)
	//TODO: I'm fixing this up...
	if(status & ((1 << 1) | (1 << 22))) {
>>>>>>> Stashed changes
		while(TRUE) {
			// take care of tx
			if(spi_info[port].txBuffer != NULL) {
				if(spi_info[port].txCurrentLen < spi_info[port].txTotalLen)
				{
					int toTX = spi_info[port].txTotalLen - spi_info[port].txCurrentLen;
<<<<<<< Updated upstream
					int canTX = (MAX_TX_BUFFER - TX_BUFFER_LEFT(status));
=======
	//TODO: fix, this is a test				int canTX = MAX_TX_BUFFER - TX_BUFFER_LEFT(status);
					int canTX = 16 - TX_BUFFER_LEFT(status);
>>>>>>> Stashed changes

					if(toTX > canTX)
						toTX = canTX;

					spi_txdata(port, spi_info[port].txBuffer, spi_info[port].txCurrentLen, spi_info[port].txCurrentLen+toTX);
					spi_info[port].txCurrentLen += toTX;

				} else {
					spi_info[port].txDone = TRUE;
					spi_info[port].txBuffer = NULL;
#ifdef CONFIG_IPOD2G
					spi_info[port].setupOptions &= ~SPISETUP_UNKN3;
					SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif
				}
			}

dorx:
			// take care of rx
			if(spi_info[port].rxBuffer == NULL)
				break;

			int toRX = spi_info[port].rxTotalLen - spi_info[port].rxCurrentLen;
<<<<<<< Updated upstream
			int canRX = GET_BITS(status, 8, 4) << spi_info[port].wordSize;
=======
			int canRX = RX_BUFFER_LEFT(status);
>>>>>>> Stashed changes

			if(toRX > canRX)
				toRX = canRX;

			spi_rxdata(port, spi_info[port].rxBuffer, spi_info[port].rxCurrentLen, spi_info[port].rxCurrentLen+toRX);
			spi_info[port].rxCurrentLen += toRX;

			if(spi_info[port].rxCurrentLen < spi_info[port].rxTotalLen)
				break;

			spi_info[port].rxDone = TRUE;
			spi_info[port].rxBuffer = NULL;
<<<<<<< Updated upstream
=======
#ifdef CONFIG_IPOD2G
			spi_info[port].setupOptions &= ~SPISETUP_UNKN1;
			SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
#endif
>>>>>>> Stashed changes
		}
	} else  if(status & (1 << 0)) {
		// jump into middle of the loop to handle rx only, stupidly
		goto dorx;
	}
#endif

#ifdef CONFIG_IPOD2G
	if (spi_info[port].rxBuffer == NULL && spi_info[port].txBuffer == NULL) {
		spi_info[port].setupOptions &= ~SPISETUP_UNKN2;
		SET_REG(SPIRegs[port].setup, spi_info[port].setupOptions);
	}
#endif
	bufferPrintf(",");
*/
}

