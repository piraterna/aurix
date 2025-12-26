/*********************************************************************************/
/* Module Name:  uefi_sfs.c */
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

#ifdef AXBOOT_UEFI

#include <efi.h>
#include <efilib.h>
#include <fs/uefi_sfs.h>
#include <lib/string.h>
#include <mm/mman.h>
#include <print.h>
#include <vfs/drive.h>
#include <vfs/vfs.h>

struct sfs_fsdata {
	EFI_FILE_PROTOCOL *volume;
};

struct vfs_drive *sfs_init(char *mountpoint)
{
	(void)mountpoint;

	EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
	EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *iovolume;
	EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_FILE_PROTOCOL *volume;
	EFI_STATUS status = EFI_SUCCESS;

	status = gBootServices->HandleProtocol(gImageHandle, &lip_guid,
										   (void **)&loaded_image);
	if (EFI_ERROR(status)) {
		debug("sfs_init(): Failed to open volume: %s (%lx)\n",
			  efi_status_to_str(status), status);
		return NULL;
	}

	status = gBootServices->HandleProtocol(loaded_image->DeviceHandle,
										   &sfs_guid, (void *)&iovolume);
	if (EFI_ERROR(status)) {
		debug("sfs_init(): Failed to open volume: %s (%lx)\n",
			  efi_status_to_str(status), status);
		return NULL;
	}

	status = iovolume->OpenVolume(iovolume, &volume);
	if (EFI_ERROR(status)) {
		debug("sfs_init(): Failed to open volume: %s (%lx)\n",
			  efi_status_to_str(status), status);
		return NULL;
	}

	debug("sfs_init(): Opened boot volume\n");

	struct vfs_filesystem *fs =
		(struct vfs_filesystem *)mem_alloc(sizeof(struct vfs_filesystem));
	if (fs == NULL) {
		debug(
			"sfs_init(): Failed to allocate memory for filesystem structure!\n");
		return NULL;
	}

	struct sfs_fsdata *fsdata =
		(struct sfs_fsdata *)mem_alloc(sizeof(struct sfs_fsdata));
	if (fsdata == NULL) {
		debug("sfs_init(): Failed to allocate memory for private data!\n");
		mem_free(fs);
		return NULL;
	}

	fsdata->volume = volume;

	fs->fsdata = fsdata;
	fs->read = sfs_read;
	fs->write = sfs_write;

	struct vfs_drive *drive =
		(struct vfs_drive *)mem_alloc(sizeof(struct vfs_drive));
	if (drive == NULL) {
		debug("sfs_init(): Failed to allocate memory for drive structure!\n");
		mem_free(fsdata);
		mem_free(fs);
		return NULL;
	}

	drive->fs = fs;
	drive->read = NULL;
	drive->write = NULL;

	return drive;
}

size_t sfs_read(char *filename, char **buffer, size_t *size,
				struct vfs_drive *dev, void *fsdata)
{
	(void)dev;

	struct sfs_fsdata *data = (struct sfs_fsdata *)fsdata;
	EFI_FILE_PROTOCOL *volume = data->volume;
	EFI_FILE_PROTOCOL *file;
	EFI_FILE_INFO *fileinfo;
	EFI_GUID fi_guid = EFI_FILE_INFO_GUID;
	EFI_UINTN fileinfo_size = sizeof(EFI_FILE_INFO) + 32;
	CHAR16 *wfilename;
	EFI_STATUS status = EFI_SUCCESS;
	size_t len = 0;

	wfilename = (CHAR16 *)mem_alloc(strlen(filename) * sizeof(CHAR16));
	if (!wfilename) {
		debug("sfs_read(): Failed to allocate memory for wide strings!\n");
		return 0;
	}

	size_t n = mbstowcs(wfilename, (const char **)&filename, strlen(filename));
	wfilename[n] = L'\0';

	/* open the file */
	status = volume->Open(volume, &file, wfilename, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status)) {
		debug("sfs_read(): Failed to open file '%s': %s (%lx)\n", filename,
			  efi_status_to_str(status), status);
		mem_free(wfilename);
		return 0;
	}

	mem_free(wfilename);

	/* get file size */
	fileinfo = (EFI_FILE_INFO *)mem_alloc(fileinfo_size);
	if (!fileinfo) {
		debug("sfs_read(): Failed to allocate memory for file info!");
		while (1)
			;
	}

	file->GetInfo(file, &fi_guid, &fileinfo_size, fileinfo);
	len = fileinfo->FileSize;
	mem_free(fileinfo);

	if (size)
		*size = len;

	*buffer = (char *)mem_alloc(len * sizeof(char));
	if (!*buffer) {
		debug("sfs_read(): Failed to allocate memory for output buffer!\n");
		return 0;
	}

	status = file->Read(file, &len, *buffer);
	if (EFI_ERROR(status)) {
		debug("sfs_read(): Failed to read file '%s': %s (%lx)\n", filename,
			  efi_status_to_str(status), status);
		return 0;
	}

	/* close the file */
	file->Close(file);

	return len;
}

uint8_t sfs_write(char *filename, char *buffer, size_t size,
				  struct vfs_drive *dev, void *fsdata)
{
	(void)dev;

	struct sfs_fsdata *data = (struct sfs_fsdata *)fsdata;
	EFI_FILE_PROTOCOL *volume = data->volume;
	EFI_FILE_PROTOCOL *file;
	CHAR16 *wfilename;
	EFI_STATUS status = EFI_SUCCESS;
	size_t len = 0;

	wfilename = (CHAR16 *)mem_alloc(strlen(filename) * sizeof(CHAR16));
	if (!wfilename) {
		debug("sfs_write(): Failed to allocate memory for wide strings!\n");
		return 0;
	}

	size_t n = mbstowcs(wfilename, (const char **)&filename, strlen(filename));
	wfilename[n] = L'\0';

	/* open the file */
	status = volume->Open(volume, &file, wfilename,
						  EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
							  EFI_FILE_MODE_CREATE,
						  EFI_FILE_SYSTEM);
	if (EFI_ERROR(status)) {
		debug("sfs_write(): Failed to open file '%s': %s (%lx)\n", filename,
			  efi_status_to_str(status), status);
		mem_free(wfilename);
		return 0;
	}

	mem_free(wfilename);

	// set position to the end of the file
	file->SetPosition(file, 0xffffffffffffffff);

	status = file->Write(file, &size, buffer);
	if (EFI_ERROR(status)) {
		debug("sfs_write(): Failed to read file '%s': %s (%lx)\n", filename,
			  efi_status_to_str(status), status);
		return 0;
	}

	/* close the file */
	file->Close(file);

	return len;
}

#endif