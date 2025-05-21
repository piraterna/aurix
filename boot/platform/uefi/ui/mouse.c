/*********************************************************************************/
/* Module Name:  mouse.c                                                         */
/* Project:      AurixOS                                                         */
/*                                                                               */
/* Copyright (c) 2024-2025 Jozef Nagy                                            */
/*                                                                               */
/* This source is subject to the MIT License.                                    */
/* See License.txt in the root of this repository.                               */
/* All other rights reserved.                                                    */
/*                                                                               */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   */
/* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, */
/* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE */
/* SOFTWARE.                                                                     */
/*********************************************************************************/

#include <ui/ui.h>
#include <ui/mouse.h>
#include <efi.h>
#include <efilib.h>
#include <stdint.h>

extern EFI_SIMPLE_POINTER_PROTOCOL *gPointerProtocol;
extern uint16_t mouse_resx;
extern uint16_t mouse_resy;

void get_mouse(uint16_t *mouse_x, uint16_t *mouse_y, uint8_t *mouse_buttons)
{
	if (!gPointerProtocol) {
		return;
	}

	*mouse_buttons = 0;
	EFI_SIMPLE_POINTER_STATE state;
	gPointerProtocol->GetState(gPointerProtocol, &state);

	*mouse_buttons |= state.LeftButton ? LEFT_MOUSE_BUTTON : 0;
	*mouse_buttons |= state.RightButton ? RIGHT_MOUSE_BUTTON : 0;

	*mouse_x += state.RelativeMovementX / mouse_resx;
	*mouse_y += state.RelativeMovementY / mouse_resy;
}