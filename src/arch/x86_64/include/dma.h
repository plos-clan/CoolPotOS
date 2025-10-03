/*
 *
 *		dma.h
 *		Direct Memory Access Header Files
 *
 *		2025/1/9 By MicroFish
 *		Based on GPL-3.0 open source agreement
 *		Copyright © 2020 ViudiraTech, based on the GPLv3 agreement.
 *
 */

#pragma once

#ifndef INCLUDE_DMA_H_
#    define INCLUDE_DMA_H_

#    include "types/stdint.h"

/* Sending commands to the DMA controller */
void dma_start(uint8_t mode, uint8_t channel, uint32_t *address, uint32_t size);

/* Sending data using DMA */
void dma_send(uint8_t channel, uint32_t *address, uint32_t size);

/* Receiving data using DMA */
void dma_recv(uint8_t channel, uint32_t *address, uint32_t size);

#endif // INCLUDE_DMA_H_
