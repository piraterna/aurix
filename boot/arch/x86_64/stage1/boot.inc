;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Module Name:  boot.inc                                                        ;;
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

	;; Legacy booting is not ready for release yet
%ifdef BUILD_DEBUG
	;;
	;; Display an error message and halt
	;;
	mov si, sErrorUnbootable
	call PrintString
	mov si, sPressToReboot
	call PrintString
	jmp AxBootHalt
%endif
	jmp AxBootHalt
