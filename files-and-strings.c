#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif


// License: Public Domain (CC0)

//
// just some parts taken from one of my projects. this includes some files and strings utilities, which may be useful.
// ~ viindicater	(github.com/viindicater, viindicater.itch.io)
//  
// ** go to main( ) for usage info / example **
//




////
// errors
////

char *errorMessage = NULL; // for general errors that are used by multiple parts of the code
//int errorCode[] = {0, 0}; // should ignore unless your code does anything different depending on error type (i.e. almost always ignore). the 2 values are: error type, error subtype (example: (1,0) 1 is file error, 0 is  file open error). can technically also check type without this, via strncmp errorMessage. see function for specific details on error type

void set_error_message(int i, int j, char *s){ // set error in one place, for consistent print strings. set errorMessage to NULL to disable error message set / build / generation.
	//errorCode[0] = i; errorCode[1] = j; // disabled because almost never needed, but can enable if you need to check error type.
	if(!errorMessage){ return; } size_t len = 1024;
	if(i == 0){ // Memory Error
		if(j == 0){ snprintf(errorMessage, len, "Memory Error: allocation failed (%s).", s); }
		if(j == 1){ snprintf(errorMessage, len, "Memory Error: failed to grow buffer (%s).", s); }
		if(j == 2){ snprintf(errorMessage, len, "Memory Error: failed to trim buffer (%s).", s); } // should never actually happen because realloc to a smaller size is usually safe
	}else if(i == 1){ // File Error
		if(j == 0){ snprintf(errorMessage, len, "File Error: failed to open '%s': %s.", s, strerror(errno)); }
		if(j == 1){ snprintf(errorMessage, len, "File Error: failed to close '%s': %s.", s, strerror(errno)); }
		if(j == 2){ snprintf(errorMessage, len, "File Error: failed to read '%s': %s.", s, strerror(errno)); }
		if(j == 3){ snprintf(errorMessage, len, "File Error: failed to write '%s: %s.'.", s, strerror(errno)); }
		if(j == 4){ snprintf(errorMessage, len, "File Error: failed to remove '%s': %s.", s, strerror(errno)); }
		if(j == 5){ snprintf(errorMessage, len, "File Error: failed to rename '%s': %s.", s, strerror(errno)); }
	}else if(i == 2){ // Parse Error
		if(j == 0){ snprintf(errorMessage, len, "Parse Error: non numeric input.", s); }
		if(j == 1){ snprintf(errorMessage, len, "Parse Error: out of range (overflow).", s); }
	}
}

/*char *readline(FILE *fp){ // original version, fgetc, unoptimal
	size_t size = 255; // initial buffer size
	size_t len = 0; char *buffer = malloc(size); int c;
	if(!buffer){ return NULL; } // allocation failure
	while((c = fgetc(fp)) != EOF && c != '\n'){
		if (len + 1 >= size){
			size *= 2;
			char *tmp = realloc(buffer, size);
			if(!tmp){ free(buffer); return NULL; } // realloc failed, cleanup
			buffer = tmp;
		}
		buffer[len++] = (char)c;
	}
	if (c == EOF) {
		if (ferror(fp)) { free(buffer); return NULL; } // I/O error (errno may give more detail)
		if(len == 0){ free(buffer); return NULL; } // no input or end of file
	}
	buffer[len] = '\0'; // null-terminate
	char *tmp = realloc(buffer, len + 1); // trim buffer to exact length
	if(!tmp){ free(buffer); return NULL; } // realloc failed, cleanup (even tho buffer is valid)
	return tmp;
}*/

////
// strings
////

char *readline(FILE *fp){ // should not be used for binary data / where null bytes are allowed due to strlen
	size_t size = 256; size_t len = 0;
	char *buffer = malloc(size);
	if (!buffer){ return NULL; }
	while(1){
		if(len + 1 >= size){ // ensure space for at least 1 char + null terminator
			size *= 2;
			char *tmp = realloc(buffer, size);
			if(!tmp){ free(buffer); return NULL; }
			buffer = tmp;
		}
		int chunk = (size - len > (size_t)INT_MAX) ? INT_MAX : (int)(size - len);
		if(!fgets(buffer + len, chunk, fp)){
			if(len == 0 || ferror(fp)){ free(buffer); return NULL; } // if EOF at start, or error
			break;
		}
		len += strlen(buffer + len); // increment len by length of the newly added segment
		if(len > 0 && buffer[len - 1] == '\n'){ buffer[--len] = '\0'; break; } // at the end, remove the newline and decrement length
	}
	char *tmp = realloc(buffer, len + 1); // trim buffer to exact length (+ 1 null terminator)
	return tmp ? tmp : buffer;
}

char **tokenize_line(const char *line, int *count){
	size_t size = 8; *count = 0; char **t = malloc(size * sizeof(char*)); if(!t){ return NULL; }
	const char *p = line;
	while(*p){
		while(*p == ' ' || *p == '\t'){ p++; } if(!*p){ break; }
		const char *s; size_t l;
		if(*p == '"'){ p++; s = p; while(*p && *p != '"'){ p++; } l = p - s; if(*p){ p++; }}
		else{ s = p; while(*p && *p != ' '&& *p != '\t'){ p++; } l = p - s; }
		char *tok = malloc(l + 1); if(!tok){ goto fail; } for(size_t i = 0; i < l; i++){ tok[i] = s[i]; } tok[l] = '\0';
		if(*count >= size){ size *= 2; char **tmp = realloc(t, size * sizeof(char*)); if(!tmp){ free(tok); goto fail;} t = tmp; }
		t[(*count)++] = tok;
	}
	
	char **tmp;
	if(*count > 0){ tmp = realloc(t, *count * sizeof(char*)); }
	else{ tmp = realloc(t, 1 * sizeof(char*)); } // if string empty (user hit enter without typing anything), shrink to 1 (instead of keeping initial size of 8)
	
	if(!tmp){ goto fail; }
	t = tmp;
	
	return t;
	fail: for(int i = 0; i < *count; i++){ free(t[i]); } free(t); return NULL;
}

char *concatv(const char *first, ...){ // return a combined buffer of all passed strings (need to free( ))
	va_list v; size_t len = 0; const char *c;
	va_start(v, first); for(c = first; c != NULL; c = va_arg(v, const char*)){ len += strlen(c); } va_end(v);
	char *s = malloc(len + 1); if(!s){ set_error_message(0, 0, "string concatv"); fprintf(stderr, "%s\n", errorMessage); return NULL; }
	char *ptr = s; va_start(v, first); for(c = first; c != NULL; c = va_arg(v, const char*)){ len = strlen(c); memcpy(ptr, c, len); ptr += len; } va_end(v); //len = strlen(s);
	*ptr = '\0'; return s;
}

void str_to_lower(char *str){ for(int i = 0; str[i]; i++){ str[i] = tolower((unsigned char)str[i]); }}
int str_to_int(const char *s, int *out){
	if(!s || *s == '\0'){ set_error_message(0, 0, "parsing"); return 1; } // if NULL ptr or empty str
	char *end; errno = 0; long v = strtol(s, &end, 10);
	if(*end != '\0'){ set_error_message(2, 0, ""); return 1; } // didn't reach string end, i.e. string has non numeric characters (or empty)
	if(errno == ERANGE || v > INT_MAX || v < INT_MIN){ set_error_message(2, 1, ""); return 2; } // if v out of int range then overflow, errno == ERANGE if strtol itself overflows long
	if(out){ *out = (int)v; } return 0;
}
int contains_tab(const char *s){ for(int i = 0; s && s[i]; i++){ if(s[i] == '\t'){ return 1; }} return 0; }

////
// files
////

char **list_files(const char *path, int *count){ // list of files in path (should ensure path is valid directory via exists_dir before this)
	int size = 16; *count = 0; char **files = malloc(size * sizeof(char*)); if(!files){ return NULL; }
#ifdef _WIN32
	char search[MAX_PATH]; snprintf(search, sizeof(search), "%s\\*", path);
	WIN32_FIND_DATA fd; HANDLE h = FindFirstFile(search, &fd);
	if(h == INVALID_HANDLE_VALUE){ free(files); return NULL; }
	do{
		if(strcmp(fd.cFileName, ".") && strcmp(fd.cFileName, "..")){
			size_t len = strlen(fd.cFileName); char *f = malloc(len + 1); if(!f){ FindClose(h); goto fail; } memcpy(f, fd.cFileName, len + 1);
			if(*count >= size){ size *= 2; char **tmp = realloc(files, size * sizeof(char*)); if(!tmp){ free(f); FindClose(h); goto fail; } files = tmp; }
			files[(*count)++] = f;
		}
	}while(FindNextFile(h, &fd));
	if(GetLastError() != ERROR_NO_MORE_FILES){ FindClose(h); goto fail; } // check error of FindNextFile
	FindClose(h);
#else
	DIR *dir = opendir(path);
	if(!dir){ free(files); return NULL; }
	struct dirent *entry; errno = 0;
	while((entry = readdir(dir)) != NULL){
		if(strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")){
			size_t len = strlen(entry->d_name);
			char *f = malloc(len + 1); if(!f){ closedir(dir); goto fail; }
			memcpy(f, entry->d_name, len+1);
			if(*count >= size){ size*=2; char **tmp = realloc(files, size * sizeof(char*)); if(!tmp){ free(f); closedir(dir); goto fail;} files = tmp; }
			files[(*count)++] = f;
		}
	}if(errno != 0){ closedir(dir); goto fail; } // check error of readdir
	closedir(dir);
#endif
	if(*count > 0){ char **tmp = realloc(files, *count * sizeof(char*)); if(tmp){ files = tmp; }} // trim, keep original if fail
	return files;
	fail: for(int i=0;i<*count;i++){ free(files[i]); } free(files); return NULL;
}

int exists_file(const char *path){ struct stat st; return stat(path, &st) == 0 && !S_ISDIR(st.st_mode); }
int exists_dir(const char *path){ struct stat st; return stat(path, &st) == 0 && S_ISDIR(st.st_mode); }

int save_file(char *name, char **lines, size_t count){
	size_t len = strlen(name); char temp_name[len + 6]; // 5 for ".temp" + 1 for "\0"
	memcpy(temp_name, name, len); memcpy(temp_name + len, ".temp", 6); // Copies ".temp" and the \0
	int error = 0; FILE *f = fopen(temp_name, "w");
	if(!f){ set_error_message(1, 0, name); fprintf(stderr, "%s\n", errorMessage); error = 1; } // failed to open
	else{
		for(int i = 0; i < count; i++){ if(fprintf(f, "%s\n", lines[i]) < 0){ set_error_message(1, 3, temp_name); fprintf(stderr, "%s\n", errorMessage); error = 1; break; }}
		if(fclose(f) != 0){ set_error_message(1, 1, name); fprintf(stderr, "%s\n", errorMessage); error = 1; }
	}if(error == 0){
		remove(name); // windows requires it to rename
		if(rename(temp_name, name) != 0){ set_error_message(1, 5, temp_name); fprintf(stderr, "%s\n", errorMessage); error = 1; } // failed to replace file with updated temp file
	}else{ remove(temp_name); } // remove temp (cleanup) if failed to write
	return error;
}

int read_file(char *name, char ***lines, size_t *count){
	size_t len = strlen(name); char temp_name[len + 6]; // 5 for ".temp" + 1 for "\0"
	memcpy(temp_name, name, len); memcpy(temp_name + len, ".temp", 6); // Copies ".temp" and the \0
	int error = 0; *count = 0; *lines = NULL; FILE *f = fopen(name, "r");
	if(!f && errno == ENOENT){ f = fopen(temp_name, "r"); } // if save_file failed to rename temp file
	if(f){
		char *buffer;
		while((buffer = readline(f)) != NULL){
			char **tmp = realloc(*lines, (*count + 1) * sizeof(char*));
			if(!tmp){ free(buffer); set_error_message(0, 1, "file read"); fprintf(stderr, "%s\n", errorMessage); error = 1; break; }
			else{
				*lines = tmp;
				(*lines)[(*count)++] = buffer;
			}
		}
		if(ferror(f)){ set_error_message(1, 2, name); fprintf(stderr, "%s\n", errorMessage); fclose(f); error = 1; } // readline error (must still close file), set f = NULL so can check if NULL to detect error
		else if(fclose(f) != 0){ set_error_message(1, 1, name); fprintf(stderr, "%s\n", errorMessage); error = 1; }
	}else if(errno != ENOENT){ /*failed to open (ignore print for file not found)*/ set_error_message(1, 0, name); fprintf(stderr, "%s\n", errorMessage); fprintf(stderr, "WARNING: If file is valid, to avoid corrupting it, should avoid taking any actions that writes to it until the above is resolved.\n"); }
	if(error == 1 && *lines){ for(size_t i = 0; i < *count; i++){ free((*lines)[i]); } free(*lines); *count = 0; *lines = NULL; };
	return error;
}


////
// fields (can ignore, is specific to my project)
////

char **parse_fields(char *line, int *count){ // WARNING: replaces \t's with \0's in the original line like strtok, but used instead of it due to empty fields i.e. \t\t\t which strtok treats as 1
	char *cur = line, *tab = NULL; int size = 5; *count = 0;
	char **fields = malloc(size * sizeof(char*));
	if(!fields){ set_error_message(0, 0, "parse fields"); fprintf(stderr, "%s\n", errorMessage); return NULL; }
	do{
		if(*count == size){
			size *= 2;
			char **tmp = realloc(fields, size * sizeof(char*));
			if(!tmp){ set_error_message(0, 1, "parse fields"); fprintf(stderr, "%s\n", errorMessage); free(fields); return NULL; }
			fields = tmp;
		}
		fields[(*count)++] = cur;
		if((tab = strchr(cur, '\t'))){ *tab = '\0'; cur = tab + 1; }
	}while(tab);
	if(*count > 0){
		char **tmp = realloc(fields, *count * sizeof(char*));
		if(tmp){ fields = tmp; }//else{ set_error_message(0, 2, "parse fields"); fprintf(stderr, "%s\n", errorMessage); free(fields); return NULL; } // no need to free each fields[i] since they are part of line, not malloc'd here
	}
	return fields;
}
void fix_line_after_fields_parse(char *line, int n){ // fixes the problem above (see the WARNING), n should be # fields - 1
	char *c = line; for(int i = 0; i < n; i++){ c += strlen(c); *c = '\t'; }
}

int main(int argc, char *argv[]){
	errorMessage = malloc(1024); if(!errorMessage){ puts("Memory Error: failed to allocate (errorMessage)."); return 1; } // enable error message generation for print (comment out to disable if you don't print). if you change malloc size here, change the size in set_error_message as well (recommended to keep 1024)
	
	/*if(1){ // Test (reads input, tokenize_line( ) , then print if each token is a valid file or directory (and list all files if directory)
		printf("(Test) Enter paths: ");
		char *line = readline(stdin); if(!line){ set_error_message(0, 0, "1"); fprintf(stderr, "%s\n", errorMessage); return 1; }
		int n; char **tok = tokenize_line(line, &n); if(!tok){ set_error_message(0, 0, "2"); fprintf(stderr, "%s\n", errorMessage); free(line); return 1; }
		for(int i = 0; i < n; i++){
			if(exists_file(tok[i])){ printf("'%s' is a file.\n",tok[i]); }
			else if(exists_dir(tok[i])){
				printf("'%s' is a directory.\n",tok[i]);
				// List contents of the directory
				int m; char **files = list_files(tok[i], &m);
				if(files){
					printf("Contents of '%s':\n", tok[i]);
					for(int j = 0; j < m; j++){ printf("%s\n", files[j]); free(files[j]); }
					free(files);
				}else{ puts("Could not read directory or empty."); }
			}
			else{ printf("'%s' does not exist.\n",tok[i]); }
			free(tok[i]);
		}
		if(line[0] == '\0'){ puts("No input."); }
		free(tok); free(line);
		puts("");
	}*/
	
	puts(""); puts("concatv example: ");
	// concatv example:
	char *cve0 = "abcd";
	char *cve = concatv("1234", cve0, NULL);
	puts(cve); free(cve);
	//
	
	
	char *fileName = "test.txt";
	
	
	// file write example
	char *saveData[] = { //normally would save the char **entries you read, but this is example data
		"A\tB\tC",
		"A\tB",
		"Q\tW\tE\t",
		"R\tT\t",
		"A",
		"Z\tX\tC",
	};
	size_t count3 = sizeof(saveData) / sizeof(saveData[0]); // number of lines
	
	
	// the actual saving is just one function call (returns 0 on success, or 1 or something else on error)
	// file and memory errors are handled by save_file itself
	if(save_file(fileName, saveData, count3) == 0){
		
		puts("Success (save).");
	
	}//else{ puts("Error: failed to save."); } // save_file itself already prints error message
		
	
	
	puts(""); puts("read file example: ");
	// file read example
	char **entries2 = NULL; size_t count2 = 0;
	if(read_file(fileName, &entries2, &count2) == 0 && entries2){
		for(size_t i = 0; i < count2; i++){ printf("%s\n", entries2[i]); } // print file
	}//else{ puts("Error: failed to read file (2)."); }
	
	
	// that's it, very simple to use- file and memory errors are handled by read_file itself
	
	if(entries2){ for(size_t i = 0; i < count2; i++){ free(entries2[i]); } free(entries2); } // cleanup
	
	
	puts(""); puts("read file with validation example (removes malformed lines): ");
	// (can ignore, is a specific example similar to what I do in my project, where file is saved as lines of tab separated fields)
	// file read with validation / parse example
	char **entries = NULL; size_t count = 0;
	size_t j = 0; // last valid line (used to drop malformed lines, to avoid needing to "free(entries[i]); for(size_t j = i; j < count - 1; j++){ entries[j] = entries[j + 1]; } count--; i--;") 
	if(read_file(fileName, &entries, &count) == 0 && entries){
		for(size_t i = 0; i < count; i++){
			int n = 0; char **fields = parse_fields(entries[i], &n); if(!fields){ goto free_entries; } // WARNING: parse_fields (like strtok) changes \t's to \0's in the original string, which is why fix_line_after_fields_parse is called below, to reverse the change.
			
			if(n != 3 || fields[0][0] == '\0' || fields[1][0] == '\0'){ // Custom validation logic, in this case, check if line has exactly 3 fields and first 2 are not empty, i.e. if malformed
				free(entries[i]); printf("Warning: malformed line #%d (# fields = %d).\n", i + 1, n); // drop malformed lines, which effectively deletes them when file is saved
			}else{ entries[j++] = entries[i]; fix_line_after_fields_parse(entries[i], n - 1); } // fix line only after fields is no longer accessed
			
			free(fields);
		} count = j;
		if(count > 0){ // trim after dropping malformed lines
			char **tmp = realloc(entries, count * sizeof(char*));
			if(!tmp){ set_error_message(0, 2, "reading paths.txt in paths menu"); fprintf(stderr, "%s\n", errorMessage); goto free_entries; }
			entries = tmp;
		}else{ free(entries); entries = NULL; }
	}else{ goto free_entries; } // failed to read file
	
	free_entries: // cleanup and print
	if(entries){ for(size_t i = 0; i < count; i++){ printf("[%d] %s\n", i + 1, entries[i]); free(entries[i]); } free(entries); }
	
	printf("\nEnter anything to exit: ");
	char *line = readline(stdin); free(line);
	
	return 0;
}