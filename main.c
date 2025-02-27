// STEP 59

#include <stddef.h>
#include <sys/_types/_ssize_t.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <stdlib.h>

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define KILO_VERSION "0.0.1"
#define ABUF_INIT {NULL, 0}
#define CTRL_KEY(k) ((k) & 0x1f) // converts letter to ctrl-letter

void die(const char *s);

enum editorKey{
    ARROW_LEFT = 1000, // others constants get values 1001, 1002, 1003
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY, // moves cursor to the beginning of the line
    END_KEY, // moves cursor to the end of the line
    DELETE_KEY,
};

typedef struct{
    int size;
    char *chars;
}erow; // editor row

struct abuf{
    char *b;
    int len;
};

struct editorConfig{
    struct termios orig_termios;
    int cx, cy;
    int screenRows;
    int screenCols;
    int numrows;
    erow *row; // holds multiple lines
};

struct editorConfig E;

// Row operations

void editorAppendRow(char *s, size_t len)
{
    E.row.size = linelen;
    E.row.chars = malloc(linelen + 1);
    memcpy(E.row.chars, line, linelen);
    E.row.chars[linelen] = '\0';
    E.numrows = 1; 
}

// File IO

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  linelen = getline(&line, &linecap, fp); // dynamically allocates memory and keeps track of n byte in linecap
  if (linelen != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                           line[linelen - 1] == '\r')) // removes newline and carriage return
      linelen--; // to get rid of \n and \r ch
      editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

// VLstr

void abAppend(struct abuf *ab, const char *s, int len)
{
    char* new = realloc(ab->b, ab->len+len);
    
    if(new==NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab)
{
    free(ab->b);
}

/// Raw mode & exit terminal

void die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios)==-1) // set backs terminal orignial settings
        die("tcsetattr");
}

void enableRawMode()
{
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    
    struct termios raw = E.orig_termios;    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); // ECHO = ch typed are printed to the screen & ICANON = ch buffer, not line buffer
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// Cursor, window and editor

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  size_t i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; // \x1b[6n = ANSI escape ctrl ch to request cursor position from terminal
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break; // cursor position is "typed" by the terminal as if it were a user
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  
  if(buf[0]!='\x1b' || buf[1]!='[') return -1;
  if(sscanf(&buf[2], "%d;%d", rows, cols)!=2) return -1; // format of wsize : \x1b[12;34R
  
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) { // 1st method to get window size
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; // 2nd method is 1st isn't portable
    return getCursorPosition(rows, cols);
  }else{
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void editorMoveCursor(int key)
{
    switch(key){
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            }
            break;
        case ARROW_RIGHT:
            if (E.cx != E.screenCols - 1) {
                E.cx++;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
            E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy != E.screenRows - 1) {
            E.cy++;
            }
            break;
    }
}

void initEditor()
{
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;
    E.row = NULL;
    if(getWindowSize(&E.screenRows, &E.screenCols)==-1) die("getWindowSize");
}

// Reading and processing keys

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
        if(seq[1] >= '0' && seq[1] <= '9')
        {
            if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
            if(seq[2] == '~')
            {
                switch(seq[1]){
                    case '1': return HOME_KEY;
                    case '3': return DELETE_KEY;
                    case '4': return END_KEY;
                    case '5': return PAGE_UP;
                    case '6': return PAGE_DOWN;
                    case '7': return HOME_KEY;
                    case '8': return END_KEY;
                }
            }
        }
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
    }else if(seq[0] == '0')
    {
        switch(seq[1]){
            case 'H': return HOME_KEY;
            case 'F': return END_KEY;
        }
    }
    return '\x1b';
    }else {
      return c;
    }
}

void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4); // AEC : J clears screen and 2 clears the whole
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case HOME_KEY:
        E.cx = 0;
        break;
    case END_KEY:
        E.cx = E.screenCols-1;
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        { // {} used because times canno't be declared directly inside case
            int times = E.screenRows;
            while(times--)
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
        break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
  }
}


// Drawing to the screen

void editorDrawRows(struct abuf *ab)
{
    int y;
    for(y=0;y<E.screenRows; y++)
    {
        if(y>E.numrows) // to print '~' only if rows writtent are passed
        {
            if(E.numrows == 0 && y==E.screenRows / 3) // 1st condition to check if user doesn't display anything
            {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), 
                "Kilo editor -- version %s", KILO_VERSION);
                if(welcomelen > E.screenCols) welcomelen = E.screenCols; // if screen is too tiny
                int padding = (E.screenCols - welcomelen) / 2;
                if(padding){
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            }else{
                abAppend(ab, "~", 1);
            }
        }else{
            int len = E.row.size;
            if (len > E.screenCols) len = E.screenCols; // truncate rendered line
            abAppend(ab, E.row.chars, len);
        }
        abAppend(ab, "\x1b[K", 3); // AEC to clear line to the right of the cursor
        if(y<E.screenRows-1) // otherwise last line got no 
            abAppend(ab, "\r\n", 2);
    }
}



void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6); // AEC (ANSI esc ch) to hide cursor
  abAppend(&ab, "\x1b[H", 3); // AEC to move cursor to upper left
  editorDrawRows(&ab);
  
  char buff[32];
  snprintf(buff, sizeof(buff), "\x1b[%d;%dH", // AEC to move cursor to position cx;cy
      E.cy+1, E.cx+1); // +1 because terminal is 1-indexed, not 0-indexed
  abAppend(&ab, buff, strlen(buff));
  
  abAppend(&ab, "\x1b[?25h", 6); // AEC to show cursor
  write(STDOUT_FILENO, ab.b, ab.len); // write whole buff to the screen
  abFree(&ab);
}


int main(int argc, char *argv[])
{
    enableRawMode();
    initEditor();
    if(argc>=2){
        editorOpen(argv[1]);
    }
    
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
      
    return 0;
}