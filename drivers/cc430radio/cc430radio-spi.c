/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_cc430radio
 * @{
 *
 * @file
 * @brief       TI Chipcon CC110x spi driver
 *
 * @author      Thomas Hillebrandt <hillebra@inf.fu-berlin.de>
 * @author      Heiko Will <hwill@inf.fu-berlin.de>
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Joakim Gebart <joakim.gebart@eistec.se>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @}
 */

#include <stdio.h>

#include "cc430radio.h"
#include "cc430radio/cc430radio-spi.h"
#include "cc430radio/cc430radio-internal.h"
#include "cc430radio/cc430radio-defines.h"
#include "cc430f5137.h"

#include "xtimer.h"
#include "irq.h"

/**********************************************************************
 *                      CC110x spi access
 **********************************************************************/

void cc430radio_cs(cc430radio_t *dev)
{
;//Chill.
}

void cc430radio_writeburst_reg(cc430radio_t *dev, uint8_t addr, const char *buffer, uint8_t count)
{
    unsigned int cpsr;
    unsigned char i;
    cpsr = disableIRQ();
      if(count > 0)
  {
    while (!(RF1AIFCTL1 & RFINSTRIFG));       // Wait for the Radio to be ready for next instruction
    RF1AINSTRW = ((addr | RF_REGWR)<<8 ) + buffer[0]; // Send address + Instruction
  
    for (i = 1; i < count; i++)
    {
      RF1ADINB = buffer[i];                   // Send data
      while (!(RFDINIFG & RF1AIFCTL1));       // Wait for TX to finish
    } 
    i = RF1ADOUTB;                            // Reset RFDOUTIFG flag which contains status byte  
  }
    restoreIRQ(cpsr);
}

void cc430radio_readburst_reg(cc430radio_t *dev, uint8_t addr, char *buffer, uint8_t count)
{
    unsigned int i = 0;
    unsigned int cpsr;
    cpsr = disableIRQ();
  if(count > 0)
  {
    while (!(RF1AIFCTL1 & RFINSTRIFG));       // Wait for INSTRIFG
    RF1AINSTR1B = (addr | RF_REGRD);          // Send addr of first conf. reg. to be read 
                                              // ... and the burst-register read instruction
    for (i = 0; i < (count-1); i++)
    {
      while (!(RFDOUTIFG&RF1AIFCTL1));        // Wait for the Radio Core to update the RF1ADOUTB reg
      buffer[i] = RF1ADOUT1B;                 // Read DOUT from Radio Core + clears RFDOUTIFG
                                              // Also initiates auo-read for next DOUT byte
    }
    buffer[count-1] = RF1ADOUT0B;             // Store the last DOUT from Radio Core  
  }		
    restoreIRQ(cpsr);
}

void cc430radio_write_reg(cc430radio_t *dev, uint8_t addr, uint8_t value)
{
    unsigned int cpsr;
    cpsr = disableIRQ();
  while (!(RF1AIFCTL1 & RFINSTRIFG));       // Wait for the Radio to be ready for next instruction
  RF1AINSTRB = (addr | RF_SNGLREGWR);	    // Send address + Instruction
  RF1ADINB = value; 			    // Write data in 
  __no_operation(); 
    restoreIRQ(cpsr);
}

uint8_t cc430radio_read_reg(cc430radio_t *dev, uint8_t addr)
{
	unsigned char data_out;
    unsigned int cpsr;
    cpsr = disableIRQ();
	  // Check for valid configuration register address, 0x3E refers to PATABLE 
  if ((addr <= 0x2E) || (addr == 0x3E))
    // Send address + Instruction + 1 dummy byte (auto-read)
    RF1AINSTR1B = (addr | RF_SNGLREGRD);    
  else
    // Send address + Instruction + 1 dummy byte (auto-read)
    RF1AINSTR1B = (addr | RF_STATREGRD);    
  
  while (!(RF1AIFCTL1 & RFDOUTIFG) );
  data_out = RF1ADOUTB;                    // Read data and clears the RFDOUTIFG
    restoreIRQ(cpsr);
    return (uint8_t) data_out;
}

uint8_t cc430radio_read_status(cc430radio_t *dev, uint8_t addr)
{
    return (uint8_t) cc430radio_strobe(dev,addr);
}

uint8_t cc430radio_get_reg_robust(cc430radio_t *dev, uint8_t addr)
{
    return (uint8_t) cc430radio_read_reg(dev,addr);
}

uint8_t cc430radio_strobe(cc430radio_t *dev, uint8_t strobe)
{
	unsigned char statusByte = 0;
	unsigned int  gdo_state;
    //unsigned int cpsr;
#ifdef CC430RADIO_DONT_RESET
    if (c == CC430RADIO_SRES) {
        return 0;
    }
#endif
    //cpsr = disableIRQ();
  // Check for valid strobe command 
  if((strobe == 0xBD) || ((strobe >= RF_SRES) && (strobe <= RF_SNOP)))
  {
    // Clear the Status read flag 
    RF1AIFCTL1 &= ~(RFSTATIFG);    
    
    // Wait for radio to be ready for next instruction
    while( !(RF1AIFCTL1 & RFINSTRIFG));
    
    // Write the strobe instruction
    if ((strobe > RF_SRES) && (strobe < RF_SNOP))
    {
      gdo_state = cc430radio_read_reg(dev,IOCFG2);    // buffer IOCFG2 state
      cc430radio_write_reg(dev,IOCFG2, 0x29);         // chip-ready to GDO2
      
      RF1AINSTRB = strobe; 
      if ( (RF1AIN&0x04)== 0x04 )           // chip at sleep mode
      {
        if ( (strobe == RF_SXOFF) || (strobe == RF_SPWD) || (strobe == RF_SWOR) ) { }
        else  	
        {
          while ((RF1AIN&0x04)== 0x04);     // chip-ready ?
          // Delay for ~810usec at 1.05MHz CPU clock, see erratum RF1A7
          __delay_cycles(850);	            
        }
      }
      cc430radio_write_reg(dev,IOCFG2, gdo_state);    // restore IOCFG2 setting
    
      while( !(RF1AIFCTL1 & RFSTATIFG) );
    }
    else		                    // chip active mode (SRES)
    {	
      RF1AINSTRB = strobe; 	   
    }
    statusByte = RF1ASTATB;
  }
    //restoreIRQ(cpsr);
    return (uint8_t) statusByte;
}
