/*********************************************************************************/
/* Module Name:  ps2.h */
/* Project:      AurixOS */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy */
/*                                                                               */
/* This source is subject to the MIT License. */
/* See License.txt in the root of this repository. */
/* All other rights reserved. */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 */
/* SOFTWARE. */
/*********************************************************************************/

#ifndef _PS2_H
#define _PS2_H

#include <stdbool.h>
#include <stdint.h>

#define PS2_DATA 0x60
#define PS2_COMMAND 0x64
#define PS2_STATUS 0x64

bool ps2_wait_until_inbuf_free();
bool ps2_send(uint8_t port, uint8_t val);
uint8_t ps2_recv(uint8_t port);

void ps2_reset_port(uint8_t port);
void ps2_reset_all();

#endif /* _PS2_H */
