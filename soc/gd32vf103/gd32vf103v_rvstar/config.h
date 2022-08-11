/*
 * Copyright (c) 2019 Nuclei Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h" /* Must come first. */
#include "queue.h"    /* RTOS queue related API prototypes. */
#include "task.h"     /* RTOS task related API prototypes. */

/* other library header file includes */
#include "nuclei_sdk_soc.h"
#include "debug.h"
#include "assert.h"

#define RVL_ASSERT_EN

#define RVL_DEBUG_EN

#define RVL_TARGET_CONFIG_REG_WIDTH         (32)
#define RVL_TARGET_CONFIG_REG_NUM           (33)
#define RVL_TARGET_CONFIG_ADDR_WIDTH        (32)

#define RISCV_DEBUG_SPEC_VERSION_V0P13

#define RVL_TARGET_CONFIG_RISCV_FLEN        (8)

#define GDB_DATA_CACHE_SIZE                 (1024)
#define GDB_PACKET_BUFF_SIZE                (1024 + 8)
#define GDB_PACKET_BUFF_NUM                 (3)

/* JTAG TCK pin definition */
#define RVL_LINK_TCK_PORT                   GPIOA
#define RVL_LINK_TCK_PIN                    GPIO_PIN_4 /* PA4, JTCK */

/* JTAG TMS pin definition */
#define RVL_LINK_TMS_PORT                   GPIOB
#define RVL_LINK_TMS_PIN                    GPIO_PIN_15 /* PB15, JTMS */

/* JTAG TDI pin definition */
#define RVL_LINK_TDI_PORT                   GPIOB
#define RVL_LINK_TDI_PIN                    GPIO_PIN_14 /* PB14, JTDI */

/* JTAG TDO pin definition */
#define RVL_LINK_TDO_PORT                   GPIOB
#define RVL_LINK_TDO_PIN                    GPIO_PIN_13 /* PB13, JTDO */

/* JTAG SRST pin definition */
#define RVL_LINK_SRST_PORT                  GPIOB
#define RVL_LINK_SRST_PIN                   GPIO_PIN_12 /* PB12, SRST */

#define RVL_JTAG_TCK_FREQ_KHZ               (1000)

#if RVL_JTAG_TCK_FREQ_KHZ >= 1000
#define RVL_JTAG_TCK_HIGH_DELAY             (30)
#define RVL_JTAG_TCK_LOW_DELAY              (1)
#elif RVL_JTAG_TCK_FREQ_KHZ >= 500
#define RVL_JTAG_TCK_HIGH_DELAY             (30 + 50)
#define RVL_JTAG_TCK_LOW_DELAY              (1 + 50)
#elif RVL_JTAG_TCK_FREQ_KHZ >= 200
#define RVL_JTAG_TCK_HIGH_DELAY             (30 + 200)
#define RVL_JTAG_TCK_LOW_DELAY              (1 + 200)
#else // 100KHz
#define RVL_JTAG_TCK_HIGH_DELAY             (30 + 450)
#define RVL_JTAG_TCK_LOW_DELAY              (1 + 450)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H__ */
