;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Module Name:  boot-hdd.asm                                                    ;;
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

_start:
	jmp short AxBootEntry
	nop

;;
;; BIOS bootloader on x86_64 is just a placeholder
;; incase it ever becomes a thing... for now it just
;; notifies the user that you can't boot AurixOS
;; without UEFI on x86_64.
;;

%include "boot.inc"
%include "print.inc"
%include "diskutils.inc"

%include "strings.inc"

AxBootHalt:
	hlt
	jmp AxBootHalt

times 510-($-$$) db 0
dw 0xaa55