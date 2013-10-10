/* +++ Start of Common Definitions (Applied to both ASIC and FPGA emulation) +++ */ 
#define CONFIG_AR7240 1
#define CONFIG_SUPPORT_AR7241 1
/* --- End   of Common Definitions (Applied to both ASIC and FPGA emulation)--- */

#ifdef _ROM_
#define CONFIG_WASP_EMU 0 
#define DV_DBG_FUNC 1
#else
/* +++ Start of FPGA Emulation Definitions (Applied to FPGA emulation) +++ */ 
/* For all FPGA emulation bit files including that for WLAN (For DV, this definition should be commented out) */
#define CONFIG_WASP_EMU 1
#define CONFIG_WASP_EMU 1

/* For WLAN FPGA emulation bit file only (For other FPGA FPGA emulation bit files and DV, this definition should be commented out) */
//#define CONFIG_WASP_EMU_HARDI_WLAN 1
/* --- End   of FPGA Emulation Definitions (Applied to FPGA emulation) --- */ 

/* +++ Start of DV Simulation Definitions (Applied to DV) +++ */ 
/* For DV debugging */
#define DV_DBG_FUNC 1

/* Switch CPU clock rate to 400MHz (This piece of code is for DV test only, and it will be placed in the 1st-downloaded firmware. This definition should be commented out) */
//#define CONFIG_PLL 1
/* --- End   of DV Simulation Definitions (Applied to DV) --- */ 
#endif /* !_ROM_ */

/* Automatically generated - do not edit */
#include <configs/waspemu.h>
