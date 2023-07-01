/*
 * INCLUDES
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * MACROS
 */

/* Minimun number of arguments for tar command */
#define	MIN_NUM_OF_ARGUMENTS		2

/* Message preffix */
#define	MSG_PREFFIX					"mytar:"

/* Options of tar command */
#define	OPT_FILENAME				"-f"
#define	OPT_LIST_FILES				"-t"

/* Block size of blocks of an archive */
#define	BLOCKSIZE_BYTES				512

/* Maximun size of file name */
#define	SIZE_NAME_MAX				100

/* Maximum number of files not found */
#define	MAX_FILES_NOT_FOUND			100

/* Octal base */
#define	OCTAL_BASE					8

/* Error codes */
#define	ERROR_CODE_TWO				2

/* Values for typeflag field */
/* Regular file */
#define	REGTYPE						'0'
/* Regular file */
#define	AREGTYPE					'\0'
/* Link */
#define	LNKTYPE						'1'
/* Reserved */
#define	SYMTYPE						'2'
/* Character special */
#define	CHRTYPE						'3'
/* Block special */
#define	BLKTYPE						'4'
/* Directory */
#define	DIRTYPE						'5'
/* FIFO special */
#define	FIFOTYPE					'6'
/* Reserved */
#define	CONTTYPE					'7'
/* Extended header referring to the next file in the archive */
#define	XHDTYPE						'x'
/* Global extended header */
#define	XGLTYPE						'g'

/* Value for magic field for a tar file */
#define	TAR_MAGIC "ustar  \0"

/*
 * GLOBAL VARIABLES
 */
typedef struct header {
    union {
		struct {
			char name[100];
			char mode[8];
			char uid[8];
			char gid[8];
			char size[12];
			char mtime[12];
			char chksum[8];
			char typeflag;
			char linkname[100];
			char magic[6];
			char version[2];
			char uname[32];
			char gname[32];
			char devmajor[8];
			char devminor[8];
			char prefix[155];
		};
		char block[BLOCKSIZE_BYTES];
	};
	struct header * next;
} header_t;

typedef struct file {
	char *name;
	char *data;
	size_t sizeData;
	struct file *next;
} file_t;

/*
 * FUNCTIONS PROTOTYPES
 */
header_t *createHeader();
file_t *createFile(char *fileName);
file_t *createFileWithData(char *fileName, char *buffer, size_t dataSize);
header_t *addHeader(header_t *list, header_t *new);
file_t *addFile(file_t *listFiles, file_t *new);
void printNameHeaders(header_t *list);
void printNameFiles(file_t *listFiles, int filesNotFoundCount);
void printNameFilesExtracted(file_t *list);
bool isZeroBlock(header_t *header);
int countBytesToSkip(header_t *header);
int findFile(char *fileName, header_t *list);
bool isFileInsideTar(char *fileName, file_t *filesInsideArchive);
void sortFileList(file_t **ptrFilesFound);
void orderFilesIndex(char *argv[], int argc);
int checkTruncatedFile(FILE *tarArchive);
bool isTarFile(char *magicField);
file_t *getFile(char *fileName, file_t *filesInsideArchive);
void freeList(header_t *list);
void freeFileList(file_t *list);

/*
 * FUNCTIONS
 */

header_t *createHeader() {
	header_t *new = NULL;
	new = (header_t *) malloc(sizeof (header_t));
	if (new == NULL) {
		free(new);
		exit(EXIT_FAILURE);
	}
	new->next = NULL;
	return (new);
}

file_t *createFile(char *fileName) {
	file_t * new = NULL;
	new = (file_t *) malloc(sizeof (file_t));
	if (new == NULL) {
		free(new);
		exit(EXIT_FAILURE);
	}
	new->name = (char *)malloc(strlen(fileName) + 1);
	if (new->name == NULL) {
		free(new->name);
		exit(EXIT_FAILURE);
	}
	strcpy(new->name, fileName);
	new->next = NULL;
	return (new);
}

file_t *createFileWithData(char *fileName, char *buffer, size_t dataSize) {
	file_t *new = NULL;
	new = (file_t *) malloc(sizeof (file_t));
	if (new == NULL) {
		free(new);
		exit(EXIT_FAILURE);
	}
	new->name = (char *)malloc(strlen(fileName) + 1);
	if (new->name == NULL) {
		free(new->name);
		exit(EXIT_FAILURE);
	}
	strcpy(new->name, fileName);
	new->data = malloc(dataSize);
	if (new->data == NULL) {
		free(new->data);
		exit(EXIT_FAILURE);
	}
	memcpy(new->data, buffer, dataSize);
	new->next = NULL;
	new->sizeData = dataSize;
	return (new);
}

header_t *addHeader(header_t *list, header_t *new) {
	if (list == NULL)
		list = new;
	else {
		header_t *aux = list;
		while (aux->next != NULL) {
			aux = aux -> next;
		}
		aux->next = new;
	}
	return (list);
}

file_t *addFile(file_t *listFiles, file_t *new) {
	if (listFiles == NULL)
		listFiles = new;
	else {
		file_t *aux = listFiles;
		while (aux->next != NULL) {
			aux = aux->next;
		}
		aux->next = new;
	}
	return (listFiles);
}

void printNameHeaders(header_t *list) {
	header_t *aux = list;
	while (aux != NULL) {
		printf("%s\n", aux->name);
		aux = aux->next;
	}
}

/*
 * this method prints the names of the files stored in a linked list.
 * If filesNotFoundCount is greater than zero, the names are printed
 * in the standard error stream. Otherwise, they are printed in the
 * standard output stream.
 */
void printNameFiles(file_t *listFiles, int filesNotFoundCount) {
    file_t *aux = listFiles;
	while (aux != NULL) {
		if (filesNotFoundCount > 0)
			fprintf(stderr, "%s\n", aux->name);
		else
			printf("%s\n", aux->name);
		aux = aux->next;
	}
}

void printNameFilesExtracted(file_t *list) {
	file_t *aux = list;
	while (aux != NULL) {
		printf("%s\n", aux->name);
		aux = aux->next;
	}
}

bool isZeroBlock(header_t *header) {
	header_t allZeroHeader;
	memset(&allZeroHeader, 0, BLOCKSIZE_BYTES);
	if (memcmp(&allZeroHeader, header, BLOCKSIZE_BYTES) == 0) {
		return (true);
	}
	return (false);
}

int countBytesToSkip(header_t *header) {
	long int contentSize = strtol(header->size, NULL, OCTAL_BASE);
	long int bytesToSkip = 0;
	if (contentSize > 0) {
		if (contentSize > BLOCKSIZE_BYTES) {
			bytesToSkip = (contentSize/BLOCKSIZE_BYTES)
							* BLOCKSIZE_BYTES;
			if ((contentSize % BLOCKSIZE_BYTES) != 0)
				bytesToSkip = ((contentSize/BLOCKSIZE_BYTES)
								* BLOCKSIZE_BYTES) + BLOCKSIZE_BYTES;
		} else if (contentSize <= BLOCKSIZE_BYTES)
			bytesToSkip = BLOCKSIZE_BYTES;
	}
	return (bytesToSkip);
}

int findFile(char *fileName, header_t *list) {
	header_t *aux = list;
	while (aux != NULL) {
		if (strcmp(fileName, aux->name) == 0)
			return (1);
		aux = aux->next;
	}
	return (0);
}

long int getBytesToRead(char *fileName, header_t *list) {
	header_t *aux = list;
	long int bytesToSkip = 0;
	while (aux != NULL) {
		if (strcmp(fileName, aux->name) == 0)
		{
			long int bytesToSkip = countBytesToSkip(aux);
			return (bytesToSkip);
		}
		aux = aux->next;
	}
	return (bytesToSkip);
}

bool isFileInsideTar(char *fileName, file_t *filesInsideArchive) {
	file_t *aux = filesInsideArchive;
	while (aux != NULL) {
		if (strcmp(fileName, aux->name) == 0)
			return (true);
		aux = aux->next;
	}
	return (false);
}

void sortFileList(file_t **ptrFilesFound) {
	if (*ptrFilesFound == NULL || (*ptrFilesFound)->next == NULL)
		return;

	file_t *current = *ptrFilesFound;
	while (current != NULL) {
		file_t *minNode = current;
		file_t *temp = current->next;
		while (temp != NULL) {
			if (strcmp(temp->name, minNode->name) < 0)
				minNode = temp;
			temp = temp->next;
		}

		if (minNode != current) {
			char *tempName = current->name;
			current->name = minNode->name;
			minNode->name = tempName;
		}
		current = current->next;
	}
}

int checkTruncatedFile(FILE *tarArchive) {
	long int currentPosition = ftell(tarArchive);
	fseek(tarArchive, 0, SEEK_END);
	long int fileSize = ftell(tarArchive);
	fseek(tarArchive, currentPosition, SEEK_SET);
	if (currentPosition > fileSize) {
		return (-1);
	}
	return (0);
}

bool isTarFile(char *magicField) {
	if (strcmp(magicField, TAR_MAGIC) == 0)
	{
		return (true);
	}
	return (false);
}

file_t *getFile(char *fileName, file_t *filesInsideArchive) {
	file_t *aux = filesInsideArchive;
	while (aux != NULL) {
		if (strcmp(fileName, aux->name) == 0) {
			file_t *foundFile = createFileWithData(fileName,
								aux->data, aux->sizeData);
			return (foundFile);
		}
		aux = aux->next;
	}
	return (NULL);
}

void freeList(header_t *list) {
	header_t *current = list;
	while (current != NULL) {
		header_t *next = current->next;
		free(current);
		current = next;
	}
}

void freeFileList(file_t *list) {
	file_t *current = list;
	while (current != NULL) {
		file_t *next = current->next;
		free(current);
		current = next;
	}
}

int main(int argc, char *argv[]) {
	if (argc < MIN_NUM_OF_ARGUMENTS)
		exit(ERROR_CODE_TWO);

	int f = 0;
	int t = 0;
	int v = 0;
	int x = 0;
	char *tarArchiveName = NULL;
	char **fileNamesArgs = NULL;
	int numFileNamesArgs = 0;

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'f':
				f = 1;
				if (strncmp(argv[i+1], "-", 1) != 0) {
					tarArchiveName = argv[i+1];
					i++;
				} else {
					printf(MSG_PREFFIX " option requires an argument -- 'f'\n"
						"Try './mytar --help' or './mytar --usage' for more"
						" information.\n");
					exit(ERROR_CODE_TWO);
				}
				break;

			case 't':
				t = 1;
				break;

			case 'v':
				v = 1;
				break;

			case 'x':
				x = 1;
				break;

			default:
				printf(MSG_PREFFIX " Unknown option: %s\n", argv[i]);
				exit(ERROR_CODE_TWO);
			}
		} else if (f && t && x) {
			printf(MSG_PREFFIX " You must specify one of the '-Acdtrux',"
				" '--delete' or '--test-label' options\n"
				"Try 'tar --help' or 'tar --usage' for more information.\n");
			exit(ERROR_CODE_TWO);
		} else if (f || t || v || x) {
			fileNamesArgs = realloc(fileNamesArgs, (numFileNamesArgs + 1) * sizeof (char *));
			if (fileNamesArgs == NULL) {
				free(fileNamesArgs);
				exit(EXIT_FAILURE);
			}
			fileNamesArgs[numFileNamesArgs] = argv[i];
			numFileNamesArgs++;
		} else {
			printf(MSG_PREFFIX " You must specify one of the '-Acdtrux',"
				" '--delete' or '--test-label' options\n"
				"Try 'tar --help' or 'tar --usage' for more information.\n");
			exit(ERROR_CODE_TWO);
		}
	}
	FILE * tarArchive = NULL;
	int filesFoundCount = 0;
	int filesNotFoundCount = 0;
	file_t *filesFound = NULL;
	file_t *filesNotFound = NULL;
	int numOptions = f + t + v + x;
	file_t *filesInsideArchive = NULL;
	bool isFileTruncated = false;

	if (v) {
		if (!x || t)
			v = 0;
		if (numOptions == 1) {
			printf(MSG_PREFFIX " You must specify one of the '-Acdtrux',"
				" '--delete' or '--test-label' options\n"
				"Try 'tar --help' or 'tar --usage' for more information.\n");
			exit(ERROR_CODE_TWO);
		}
	}

	if (f && argc == 2) {
		printf(MSG_PREFFIX " option requires an argument -- 'f'\n"
				"Try './mytar --help' or './mytar --usage' for more"
				" information.\n");
		exit(ERROR_CODE_TWO);
	} else if ((t && argc == 2) || (x && argc == 2)) {
		printf(MSG_PREFFIX " Refusing to read archive contents from terminal"
				" (missing -f option?)\n"
				MSG_PREFFIX " Error is not recoverable: exiting now\n");
		exit(ERROR_CODE_TWO);
	} else if (f && t) {
		tarArchive = fopen(tarArchiveName, "r");
		if (tarArchive == NULL) {
			printf(MSG_PREFFIX " %s file does not exist in current"
					" directory\n", argv[2]);
			exit(ERROR_CODE_TWO);
		}

		header_t * list = NULL;
		int posZeroBlock = 1;
		bool isLoneZeroBlock = false;

		while (1) {
			header_t *newHeader = createHeader();
			size_t element_read = fread((newHeader->block),
								sizeof (newHeader->block), 1, tarArchive);
			if (ferror(tarArchive)) {
				free(newHeader);
				exit(EXIT_FAILURE);
			}

			if (element_read != 1)
				break;

			if (isZeroBlock(newHeader) == true) {
				if (fread((newHeader->block),
					sizeof (newHeader->block), 1, tarArchive) != 1) {
					posZeroBlock ++;
					isLoneZeroBlock = true;
				}
				break;
			}

			if (isTarFile(newHeader->magic) == false) {
				printf(MSG_PREFFIX " This does not look like a tar archive\n");
				printf(MSG_PREFFIX " Exiting with failure status due to"
						" previous errors\n");
				exit(ERROR_CODE_TWO);
			}

			if ((newHeader->typeflag != REGTYPE) &&
				newHeader->typeflag != AREGTYPE) {
				printf(MSG_PREFFIX " Unsupported header type: %d\n",
						newHeader->typeflag);
				exit(ERROR_CODE_TWO);
			}

			list = addHeader(list, newHeader);

			long int bytesToSkip = countBytesToSkip(newHeader);
			fseek(tarArchive, bytesToSkip, SEEK_CUR);
			while (bytesToSkip > 0) {
				bytesToSkip -= BLOCKSIZE_BYTES;
				posZeroBlock++;
			}

			if (checkTruncatedFile(tarArchive) == -1) {
				printf("%s\n", newHeader->name);
				printf(MSG_PREFFIX " Unexpected EOF in archive\n");
				printf(MSG_PREFFIX " Error is not recoverable: exiting now\n");
				exit(ERROR_CODE_TWO);
			}
		}
		fclose(tarArchive);

		if (numFileNamesArgs == 0)
			printNameHeaders(list);
		else {
			for (int i = 0; i < numFileNamesArgs; i++) {
				char *fileName = fileNamesArgs[i];
				if (findFile(fileName, list) == 1) {
					file_t *newFile = createFile(fileName);
					filesFound = addFile(filesFound, newFile);
					filesFoundCount++;
				} else {
					file_t *newFile = createFile(fileName);
					filesNotFound = addFile(filesNotFound, newFile);
					filesNotFoundCount++;
				}
			}

			if (filesFoundCount > 0) {
				file_t **ptrFilesFound = &filesFound;
				sortFileList(ptrFilesFound);
				printNameFiles(filesFound, filesNotFoundCount);
			}

			if (filesNotFoundCount > 0) {
				file_t *current = filesNotFound;
				while (current != NULL) {
					fprintf(stderr, MSG_PREFFIX " %s: Not found in"
							" archive\n", current->name);
					current = current->next;
				}
				fprintf(stderr, MSG_PREFFIX " Exiting with failure status due"
						" to previous errors\n");
				exit(ERROR_CODE_TWO);
			}
		}

		if (isLoneZeroBlock == true)
			printf(MSG_PREFFIX " A lone zero block at %d\n",
					posZeroBlock);

		freeList(list);
		free(fileNamesArgs);
		freeFileList(filesFound);
		freeFileList(filesNotFound);
	}

	if (x) {
		if (numFileNamesArgs == 0) {
			tarArchive = fopen(tarArchiveName, "r");
			if (tarArchive == NULL) {
				printf(MSG_PREFFIX " %s file does not exist in current"
						" directory\n", argv[2]);
				exit(ERROR_CODE_TWO);
			}

			header_t *list = NULL;
			int posZeroBlock = 1;
			bool isLoneZeroBlock = false;

			while (1) {
				header_t *newHeader = createHeader();
				size_t element_read = fread((newHeader->block),
									sizeof (newHeader->block), 1, tarArchive);
				if (ferror(tarArchive)) {
					free(newHeader);
					exit(EXIT_FAILURE);
				}

				if (element_read != 1)
					break;

				if (isZeroBlock(newHeader) == true) {
					if (fread((newHeader->block),
						sizeof (newHeader->block), 1, tarArchive) != 1) {
						posZeroBlock ++;
						isLoneZeroBlock = true;
					}
					break;
				}

				if ((newHeader->typeflag != REGTYPE) &&
					newHeader->typeflag != AREGTYPE) {
					printf(MSG_PREFFIX " Unsupported header type:"
							" %d\n", newHeader->typeflag);
					exit(ERROR_CODE_TWO);
				}

				if (isTarFile(newHeader->magic) == false) {
					printf(MSG_PREFFIX " This does not look like a tar"
							" archive\n");
					printf(MSG_PREFFIX " Exiting with failure status due to"
							" previous errors\n");
					exit(ERROR_CODE_TWO);
				}

				list = addHeader(list, newHeader);


				long int bytesToRead = countBytesToSkip(newHeader);

				char *buffer = (char *)malloc(bytesToRead);
				if (buffer == NULL) {
					free(buffer);
					exit(EXIT_FAILURE);
				}
				size_t bytesRead = fread(buffer, sizeof (char), bytesToRead,
									tarArchive);
				if (ferror(tarArchive)) {
					free(buffer);
					exit(EXIT_FAILURE);
				}
				file_t *newFile = createFileWithData(newHeader->name, buffer,
									bytesRead);
				free(buffer);

				filesInsideArchive = addFile(filesInsideArchive, newFile);

				if ((size_t)bytesToRead > bytesRead)
					fseek(tarArchive, (bytesToRead - bytesRead), SEEK_CUR);

				if (checkTruncatedFile(tarArchive) == -1) {
					isFileTruncated = true;
					if (v)
						fprintf(stderr, "%s\n", newHeader->name);
				}

				while (bytesToRead > 0) {
					bytesToRead -= BLOCKSIZE_BYTES;
					posZeroBlock++;
				}
			}
			fclose(tarArchive);

			file_t *currentFile = filesInsideArchive;
			while (currentFile != NULL) {
				char *fileName = currentFile->name;
				char *fileData = currentFile->data;
				size_t fileSizeData = currentFile->sizeData;

				FILE *newFile = fopen(fileName, "w");
				if (newFile == NULL) {
					printf("Error creating the file: %s\n", fileName);
				} else {
					size_t elementsWritten = fwrite(fileData, sizeof (char),
												fileSizeData, newFile);
					if (elementsWritten == 0) {
						if (ferror(newFile)) {
							free(filesInsideArchive);
							fclose(newFile);
							exit(EXIT_FAILURE);
						}
					}
					fclose(newFile);
				}
				currentFile = currentFile->next;
			}

			if (isFileTruncated == true) {
				printf(MSG_PREFFIX " Unexpected EOF in archive\n");
				printf(MSG_PREFFIX " Error is not recoverable: exiting now\n");
				exit(ERROR_CODE_TWO);
			}

			if (v) {
				printNameFilesExtracted(filesInsideArchive);
				if (isLoneZeroBlock == true)
					printf(MSG_PREFFIX " A lone zero block at %d\n",
							posZeroBlock);
			} else {
				if (isLoneZeroBlock == true)
					printf(MSG_PREFFIX " A lone zero block at %d\n",
							posZeroBlock);
			}
		} else {
			tarArchive = fopen(tarArchiveName, "r");
			if (tarArchive == NULL) {
				printf(MSG_PREFFIX " %s file does not exist in current"
						" directory\n", argv[2]);
				exit(ERROR_CODE_TWO);
			}

			header_t *list = NULL;
			int posZeroBlock = 1;
			bool isLoneZeroBlock = false;

			while (1) {
				header_t *newHeader = createHeader();
				size_t element_read = fread((newHeader->block),
									sizeof (newHeader->block), 1, tarArchive);
				if (ferror(tarArchive)) {
					free(newHeader);
					exit(EXIT_FAILURE);
				}

				if (element_read != 1)
					break;

				if (isZeroBlock(newHeader) == true) {
					if (fread((newHeader->block),
						sizeof (newHeader->block), 1, tarArchive) != 1) {
						posZeroBlock ++;
						isLoneZeroBlock = true;
					}
					break;
				}

				if ((newHeader->typeflag != REGTYPE) &&
					newHeader->typeflag != AREGTYPE) {
					printf(MSG_PREFFIX " Unsupported header type:"
							" %d\n", newHeader->typeflag);
					exit(ERROR_CODE_TWO);
				}

				list = addHeader(list, newHeader);

				long int bytesToSkip = countBytesToSkip(newHeader);
				char *buffer = (char *)malloc(bytesToSkip);
				if (buffer == NULL) {
					free(buffer);
					exit(EXIT_FAILURE);
				}
				size_t bytesRead = fread(buffer, sizeof (char),
									bytesToSkip, tarArchive);
				if (ferror(tarArchive)) {
					free(buffer);
					exit(EXIT_FAILURE);
				}

				file_t *newFile = createFileWithData(newHeader->name, buffer,
													bytesRead);
				free(buffer);

				filesInsideArchive = addFile(filesInsideArchive, newFile);

				if ((size_t)bytesToSkip > bytesRead)
					fseek(tarArchive, (bytesToSkip - bytesRead), SEEK_CUR);

				if (checkTruncatedFile(tarArchive) == -1) {
					printf("%s\n", newHeader->name);
					fprintf(stderr, MSG_PREFFIX " Unexpected EOF in"
							" archive\n");
					fprintf(stderr, MSG_PREFFIX " Error is not recoverable:"
							" exiting now\n");
					exit(ERROR_CODE_TWO);
				}

				while (bytesToSkip > 0) {
					bytesToSkip -= BLOCKSIZE_BYTES;
					posZeroBlock++;
				}
			}
			fclose(tarArchive);

			for (int i = 0; i < numFileNamesArgs; i++) {
				file_t *fileWanted = NULL;
				char *fileName = fileNamesArgs[i];

				if (findFile(fileName, list) == 1) {
					FILE *newFile = fopen(fileName, "w");

					fileWanted = getFile(fileName, filesInsideArchive);

					size_t elementsWritten = fwrite(fileWanted->data,
										sizeof (char), fileWanted->sizeData,
										newFile);
					if (elementsWritten == 0) {
						if (ferror(newFile)) {
							free(filesInsideArchive);
							fclose(newFile);
							exit(EXIT_FAILURE);
						}
					}
					fclose(newFile);

					filesFound = addFile(filesFound, fileWanted);
					filesFoundCount++;
				} else {
					file_t *newFile = createFile(fileName);
					filesNotFound = addFile(filesNotFound, newFile);
					filesNotFoundCount++;
				}
			}

			if (isLoneZeroBlock == true)
				printf(MSG_PREFFIX " A lone zero block at %d\n",
						posZeroBlock);

			if (v) {
				if (filesFoundCount > 0) {
					file_t **ptrFilesFound = &filesFound;
					sortFileList(ptrFilesFound);
					printNameFilesExtracted(filesFound);
				}

				if (filesNotFoundCount > 0) {
					file_t *current = filesNotFound;
					while (current != NULL) {
						fprintf(stderr, MSG_PREFFIX " %s: Not found in"
								" archive\n", current->name);
						current = current->next;
					}
					fprintf(stderr, MSG_PREFFIX " Exiting with failure status due to"
							" previous errors\n");
					exit(ERROR_CODE_TWO);
				}

				freeList(list);
				free(fileNamesArgs);
				freeFileList(filesInsideArchive);
				freeFileList(filesFound);
				freeFileList(filesNotFound);

				if (isLoneZeroBlock == true)
					printf(MSG_PREFFIX " A lone zero block at %d\n",
							posZeroBlock);
			}
		}
	}

	return (0);
}
