/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.1
* Build Version : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
* Copyright 2020 - 2026 NXP
*
*   NXP Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file main.c
*
*   @addtogroup main_module main module documentation
*   @{
*/

/* User includes */
#include "Mcl.h"
#include "Mcu.h"
#include "CDD_Uart.h"
#include "Port.h"
#include "Platform.h"
#include "Adc.h"
#include <stdio.h>
#include "freemaster.h"
#include "freemaster_s32_lpuart.h"


/* Global variables for FreeMASTER */
float32         temperature;
bool            error_flag;

/* Required ADC group-end notification callback */
void Adc_Adc0Group0Notification(void)
{
	/* Polling mode (not used in this demo) */
}

int main(void)
{
    Adc_CalibrationStatusType  calStatus;
    uint16                     adcResult;
    Std_ReturnType             adcStatus;

    /* System init */
    Mcu_Init(NULL_PTR);
    Mcu_InitClock(0);

#if (MCU_NO_PLL == STD_OFF)
    while (MCU_PLL_LOCKED != Mcu_GetPllStatus())
    {
        /* Wait for PLL lock */
    }
    Mcu_DistributePllClock();
#endif

    Mcu_SetMode(0);
    Mcl_Init(NULL_PTR);
    Port_Init(NULL_PTR);
    Platform_Init(NULL_PTR);
    Uart_Init(NULL_PTR);
    /* Set FreeMASTER serial base address. */
	FMSTR_SerialSetBaseAddress((FMSTR_ADDR)IP_LPUART_6_BASE);
    /* FreeMASTER Initialization */
    FMSTR_Init();

    /* ADC, TempSense init */
    Adc_Init(NULL_PTR);
    Adc_Calibrate(0U, &calStatus);
    (void)calStatus;
    Adc_TempSenseSetPowerMode(ADC_NORMAL_MODE);

    /* Adc_TempSenseGetTemp() triggers an internal ADC conversion on the TempSense
     * channel, waits for completion, and writes the result to adcResult in Q11.4
     * fixed-point format (1 sign bit, 11 integer bits, 4 fractional bits).
     * Dividing by 16 converts the Q11.4 value to degrees Celsius.
     *
     * Three-stage glitch filtering:
     *   1. TEMP_SENSE_MIN_C / TEMP_SENSE_MAX_C -- absolute physical range guard.
     *   2. TEMP_SENSE_MAX_DELTA_C -- per-sample rate-of-change guard; a single
     *      sample that jumps more than this many degrees from the last accepted
     *      value is treated as a transient glitch and discarded.
     *   3. IIR low-pass filter (exponential moving average) with smoothing factor
     *      TEMP_SENSE_IIR_ALPHA -- attenuates any remaining high-frequency noise.
     *      Alpha=0.1 gives a time constant of ~9 samples (approx. 9 s at 1 s/sample).  */
#define TEMP_SENSE_MIN_C       (-40.0f)
#define TEMP_SENSE_MAX_C       (150.0f)
#define TEMP_SENSE_MAX_DELTA_C (10.0f)
#define TEMP_SENSE_IIR_ALPHA   (0.1f)

    /* Flag to detect first valid sample -- skip delta check on initialisation. */
    bool tempInitialised = FALSE;

    while (1)
    {
        adcStatus = Adc_TempSenseGetTemp(0U, &adcResult);
        if (E_OK == adcStatus)
        {
            float32 tempCandidate = (float32)((sint16)adcResult) / 16.0f;

            /* Stage 1: absolute range check. */
            if ((tempCandidate < TEMP_SENSE_MIN_C) || (tempCandidate > TEMP_SENSE_MAX_C))
            {
                error_flag = TRUE;
            }
            /* Stage 2: rate-of-change check (skip on first valid sample). */
            else if (tempInitialised &&
                     ((tempCandidate - temperature) >  TEMP_SENSE_MAX_DELTA_C ||
                      (tempCandidate - temperature) < -TEMP_SENSE_MAX_DELTA_C))
            {
                error_flag = TRUE;
            }
            else
            {
                /* Stage 3: IIR low-pass filter. */
                if (!tempInitialised)
                {
                    temperature    = tempCandidate;   /* seed the filter on first sample */
                    tempInitialised = TRUE;
                }
                else
                {
                    temperature = (TEMP_SENSE_IIR_ALPHA * tempCandidate) +
                                  ((1.0f - TEMP_SENSE_IIR_ALPHA) * temperature);
                }
                error_flag = FALSE;
            }
        }
        else
        {
            error_flag = TRUE;
        }
        FMSTR_Poll();
    }
    return 0;
}
