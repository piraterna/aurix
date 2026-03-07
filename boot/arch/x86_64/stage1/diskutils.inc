;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Module Name:  diskutils.inc                                                   ;;
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

;; dh = number of sectors to read
;; dl = drive number
;;
;; output: es:bx
LoadDisk:
	push dx
	mov ah, 0x02
	mov al, dh
	mov ch, 0x00
	mov dh, 0x00
	mov cl, 0x02
	int 0x13
	jc .ReadError
	pop dx
	cmp dh, al
	jne .ReadError
	ret
	.ReadError:
		mov si, sDiskReadError
		call PrintString
		call AxBootHalt

sDiskReadError: db 'ERROR: Failed to read disk!', 0