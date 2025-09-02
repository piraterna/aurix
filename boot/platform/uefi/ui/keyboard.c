/*********************************************************************************/
/* Module Name:  keyboard.c */
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

#include <print.h>
#include <stdint.h>
#include <ui/keyboard.h>

#include <efi.h>
#include <efilib.h>

void get_keypress(uint16_t *scancode)
{
	EFI_INPUT_KEY key;
	if (gSystemTable->ConIn->ReadKeyStroke(gSystemTable->ConIn, &key) ==
		EFI_SUCCESS) {
		if (key.UnicodeChar == L'\r') {
			*scancode = SCANCODE_ENTER;
			return;
		}
		*scancode = key.ScanCode;
	} else {
		*scancode = 0;
	}
}