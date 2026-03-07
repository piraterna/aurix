;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Module Name:  boot-cd.asm                                                     ;;
;; Project:      AurixOS                                                         ;;
;;                                                                               ;;
;; Copyright (c) 2024-2026 Jozef Nagy                                            ;;
;;                                                                               ;;
;; This source is subject to the MIT License.                                    ;;
;; See License.txt in the root of this repository.                               ;;
;; All other rights reserved.                                                    ;;
;;                                                                               ;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR    ;;
;; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,      ;;
;; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE   ;;
;; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER        ;;
;; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, ;;
;; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE ;;
;; SOFTWARE.                                                                     ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

[bits 16]
[org 0x7c00]

jmp 0x0000:AxBootEntry

times 8-($-$$) db 0

bi_PrimaryVolumeDescriptor dd 0
bi_BootFileLocation dd 0
bi_BootFileLength dd 0
bi_Checksum dd 0
bi_Reserved times 40 db 0

;;
;; BIOS bootloader on i686 is just a placeholder
;; incase it ever becomes a thing... for now it just
;; notifies the user that you can't boot AurixOS
;; without UEFI on i686.
;;

%include "arch/strings.inc"

AxBootEntry:
	;;
	;; Set 80x50 text mode and clear the screen
	;;
	mov ax, 0x03
	int 0x10
	xor bx, bx
	mov ax, 0x1112
	int 0x10
	mov ah, 0
	int 0x10

	;;
	;; Display an error message and halt
	;;
	mov si, sErrorUnbootable
	call PrintString
	mov si, sPressToReboot
	call PrintString
	jmp AxBootHalt

PrintString:
	lodsb
	or al, al
	jz .done
	mov ah, 0x0e
	mov bx, 0x0007
	int 0x10
	jmp PrintString
	.done:
		ret

AxBootHalt:
	cli
	hlt
	jmp AxBootHalt

times 510-($-$$) db 0
dw 0xaa55