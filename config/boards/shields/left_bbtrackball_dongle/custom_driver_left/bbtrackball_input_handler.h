/*
 * Copyright (c) 2023 ZitaoTech
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BBTRACKBALL_INPUT_HANDLER_H
#define BBTRACKBALL_INPUT_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief check if the trackball is moving
 *
 * @return true if moved
 */
bool trackball_is_moving(void);

#ifdef __cplusplus
}
#endif

#endif // BBTRACKBALL_INPUT_HANDLER_H
