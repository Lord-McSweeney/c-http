#include <ncurses.h>

typedef void (*clickHandler)(void *, char *);

enum nc_page {
    PAGE_DOCUMENT_LOADED,
    PAGE_GOTO_DIALOG,
    PAGE_GOTO_OVER_DOCUMENT,
    PAGE_EMPTY,
};

char *nc_strcpy(const char *text) {
    char *newText = (char *) calloc(strlen(text) + 1, sizeof(char));
    strcpy(newText, text);
    return newText;
}


struct nc_text_area {
    int y;
    int x;
    int width;
    int height;
    int visible;
    char *currentText;
    char *descriptor;
    int selected;
    int scrolledAmount;
};

struct nc_button {
    int y;
    int x;
    int visible;
    char *text;
    clickHandler onclick;
    char *descriptor;
    int selected;
    int enabled;
};

struct nc_text {
    int y;
    int x;
    int visible;
    char *text;
    char *descriptor;
};

struct nc_selected {
    struct nc_text_area *textarea;
    struct nc_button *button;
};

struct nc_selected selectableFromButton(struct nc_button *button) {
    struct nc_selected select;
    select.button = button;
    select.textarea = NULL;
    return select;
}

struct nc_selected selectableFromTextarea(struct nc_text_area *textarea) {
    struct nc_selected select;
    select.button = NULL;
    select.textarea = textarea;
    return select;
}

struct nc_state {
    enum nc_page currentPage;
    
    struct nc_text_area text_areas[256];
    struct nc_button buttons[256];
    struct nc_text texts[256];
    int numTextAreas;
    int numButtons;
    int numTexts;
    
    struct nc_selected *selectables;
    int numSelectables;
    int selectableIndex;
    
    // todo: add a forward/back buffer
};

struct nc_text_area *getCurrentSelectedTextarea(struct nc_state *state) {
    for (int i = 0; i < state->numTextAreas; i ++) {
        if (state->text_areas[i].selected && state->text_areas[i].visible) {
            return &state->text_areas[i];
        }
    }
    return NULL;
}

struct nc_button *getCurrentSelectedButton(struct nc_state *state) {
    for (int i = 0; i < state->numButtons; i ++) {
        if (state->buttons[i].selected && state->buttons[i].visible && state->buttons[i].enabled) {
            return &state->buttons[i];
        }
    }
    return NULL;
}

int isSelectable(struct nc_selected selectable) {
    if (selectable.button != NULL) {
        return selectable.button->visible && selectable.button->enabled;
    } else if (selectable.textarea != NULL) {
        return selectable.textarea->visible;
    } else {
        // Somehow both are none???
        return 0;
    }
}

struct nc_button *getButtonByDescriptor(struct nc_state *state, char *descriptor) {
    int num = state->numButtons;
    for (int i = 0; i < num; i ++) {
        if (!strcmp(descriptor, state->buttons[i].descriptor)) {
            return &state->buttons[i];
        }
    }
    /*printf("Attempted to find a button that didn't exist!\n");
    exit(0);*/
    return NULL;
}

struct nc_text_area *getTextAreaByDescriptor(struct nc_state *state, char *descriptor) {
    int num = state->numTextAreas;
    for (int i = 0; i < num; i ++) {
        if (!strcmp(descriptor, state->text_areas[i].descriptor)) {
            return &state->text_areas[i];
        }
    }
    /*printf("Attempted to find a text area that didn't exist!\n");
    exit(0);*/
    return NULL;
}

struct nc_text *getTextByDescriptor(struct nc_state *state, char *descriptor) {
    int num = state->numTexts;
    for (int i = 0; i < num; i ++) {
        if (!strcmp(descriptor, state->texts[i].descriptor)) {
            return &state->texts[i];
        }
    }
    /*printf("Attempted to find a text that didn't exist!\n");
    exit(0);*/
    return NULL;
}

struct nc_button createNewButton(struct nc_state *state, int x, int y, char *text, clickHandler onclick, char *descriptor) {
    if (getButtonByDescriptor(state, descriptor) != NULL) {
        printf("Attempted to initialize a button with duplicate descriptor!\n");
        exit(0);
    }
    struct nc_button button;
    button.x = x;
    button.y = y;
    button.visible = 1;
    button.selected = 0;
    button.enabled = 1;
    button.text = text;
    button.onclick = onclick;
    button.descriptor = nc_strcpy(descriptor);
    state->buttons[state->numButtons] = button;
    state->numButtons ++;
    
    state->selectables = (struct nc_selected *) realloc(state->selectables, (state->numSelectables + 1) * sizeof(struct nc_selected));
    state->selectables[state->numSelectables] = selectableFromButton(&state->buttons[state->numButtons - 1]);
    state->numSelectables ++;
    return button;
}

struct nc_text_area createNewTextarea(struct nc_state *state, int x, int y, int w, int h, char *descriptor) {
    if (getTextAreaByDescriptor(state, descriptor)) {
        printf("Attempted to initialize a text area with duplicate descriptor!\n");
        exit(0);
    }
    struct nc_text_area textarea;
    textarea.x = x;
    textarea.y = y;
    textarea.width = w;
    textarea.height = h;
    textarea.visible = 1;
    textarea.selected = 0;
    textarea.currentText = (char *) calloc(1024, sizeof(char));
    textarea.descriptor = nc_strcpy(descriptor);
    textarea.scrolledAmount = 0;
    state->text_areas[state->numTextAreas] = textarea;
    state->numTextAreas ++;
    
    state->selectables = (struct nc_selected *) realloc(state->selectables, (state->numSelectables + 1) * sizeof(struct nc_selected));
    state->selectables[state->numSelectables] = selectableFromTextarea(&state->text_areas[state->numTextAreas - 1]);
    state->numSelectables ++;
    return textarea;
}

struct nc_text createNewText(struct nc_state *state, int x, int y, char *givenText, char *descriptor) {
    if (getButtonByDescriptor(state, descriptor)) {
        printf("Attempted to initialize a text area with duplicate descriptor!\n");
        exit(0);
    }
    struct nc_text text;
    text.x = x;
    text.y = y;
    text.visible = 1;
    text.text = nc_strcpy(givenText);
    text.descriptor = nc_strcpy(descriptor);
    state->texts[state->numTexts] = text;
    state->numTexts ++;
    return text;
}

char *getTextAreaRendered(struct nc_text_area textarea) {
    char *resultingText = (char *) calloc(textarea.width + 2, sizeof(char));

    int under = textarea.width - strlen(textarea.currentText);
    if (under < 0) under = 0;
    for (int i = textarea.scrolledAmount; i < strlen(textarea.currentText); i ++) {
        resultingText[i - textarea.scrolledAmount] = textarea.currentText[i];
    }
    for (int i = 0; i < under; i ++) {
        resultingText[strlen(resultingText)] = '_';
    }

    return resultingText;
}

void updateFocus_nc(struct nc_state *state) {
    int numTextAreas = state->numTextAreas;
    int numButtons = state->numButtons;
    for (int i = 0; i < numButtons; i ++) {
        state->buttons[i].selected = 0;
    }
    for (int i = 0; i < numTextAreas; i ++) {
        state->text_areas[i].selected = 0;
    }
    if (state->selectableIndex == -1) {
        // no selectable
        return;
    }
    struct nc_selected curSelected = state->selectables[state->selectableIndex];
    if (curSelected.textarea == NULL) {
        struct nc_button *button = curSelected.button;
        button->selected = 1;
    } else if (curSelected.button == NULL) {
        struct nc_text_area *textarea = curSelected.textarea;
        //textarea.currentText[0] = 'Z';
        textarea->selected = 1;
    } else {
        // ...both are selected????????
    }
}

char *repeatChar(char c, int times) {
    char *mem = (char *) calloc(times + 1, sizeof(char));
    for (int i = 0; i < times; i ++) {
        mem[i] = c;
    }
    return mem;
}

void printText(int y, int x, char *text) {
    int mx;
    int my;
    getmaxyx(stdscr, my, mx);

    int realPosY = y;
    int realPosX = x;
    int minX = x;
    int len = strlen(text);
    char *dashes = repeatChar('-', mx - mx/30);
    char *spaces = repeatChar(' ', mx/60);
    char *horizontalRow = (char *) calloc(mx + 1, sizeof(char));
    strcpy(horizontalRow, spaces);
    strcat(horizontalRow, dashes);
    free(dashes);
    free(spaces);

    int escapeModeEnabled = 0;
    int italics = 0;
    int bold = 0;
    int underline = 0;
    int doIndent = 0;
    int colorsP[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int colorStackNum = 0;
    for (int i = 0; i < len; i ++) {
        if (text[i] == '\\' && !escapeModeEnabled) {
            escapeModeEnabled = 1;
            continue;
        }
        if (escapeModeEnabled) {
            int doContinue = 0;
            switch (text[i]) {
                case 'h':
                    mvaddstr(realPosY, realPosX, horizontalRow);
                    realPosX = mx;
                    break;

                case 'b':
                    bold ++;
                    if (bold > 1048576) {
                        // This limit should never be reached, but let's just make sure.
                        bold --;
                    }
                    break;
                case 'c':
                    if (bold > 0) {
                        bold --;
                    }
                    break;

                case 'i':
                    italics ++;
                    if (italics > 1048576) {
                        // This limit should never be reached, but let's just make sure.
                        italics --;
                    }
                    break;
                case 'j':
                    if (italics > 0) {
                        italics --;
                    }
                    break;

                case 'q':
                    underline ++;
                    if (underline > 1048576) {
                        // This limit should never be reached, but let's just make sure.
                        underline --;
                    }
                    break;
                case 'r':
                    if (underline > 0) {
                        underline --;
                    }
                    break;

                case 't':
                    doIndent ++;
                    if (doIndent > 1048576) {
                        // This limit should never be reached, but let's just make sure.
                        doIndent --;
                    }
                    mvaddstr(realPosY, realPosX, "    ");
                    realPosX += 4;
                    break;
                case 'u':
                    if (doIndent > 0) {
                        doIndent --;
                    }
                    break;

                case '4':
                    colorsP[colorStackNum] = 3;
                    colorStackNum ++;
                    if (colorStackNum > 62) {
                        // This limit should (hopefully) never be reached, but let's just make sure.
                        colorStackNum --;
                    }
                    break;

                case '0':
                    if (colorStackNum > 0) {
                        colorStackNum --;
                        colorsP[colorStackNum] = 0;
                    }
                    break;

                case '\\':
                    doContinue = 1;
                    break;
            }
            escapeModeEnabled = 0;
            if (!doContinue) {
                continue;
            }
        }
        if (realPosY >= 0 && realPosX >= 0 && realPosY < my) {
            int toPrint = text[i];
            if (italics) {
                toPrint = toPrint | A_ITALIC;
            }

            if (bold) {
                toPrint = toPrint | A_BOLD;
            }

            if (underline) {
                toPrint = toPrint | A_UNDERLINE;
            }

            if (colorStackNum) {
                attron(COLOR_PAIR(colorsP[colorStackNum - 1]));
            } else {
                attron(COLOR_PAIR(1));
            }

            mvaddch(realPosY, realPosX, toPrint);
        }
        realPosX ++;
        if (text[i] == '\n' || realPosX >= mx) {
            realPosX = minX;
            realPosY ++;
            if (doIndent) {
                mvaddstr(realPosY, realPosX, "    ");
                realPosX += 4;
            }
        }
    }

    free(horizontalRow);
}

void render_nc(struct nc_state *browserState) {
    clear();
    int numTexts = browserState->numTexts;
    int numTextAreas = browserState->numTextAreas;
    int numButtons = browserState->numButtons;
    for (int i = 0; i < numTexts; i ++) {
        if (browserState->texts[i].visible) {
            printText(browserState->texts[i].y, browserState->texts[i].x, browserState->texts[i].text);
        }
    }
    curs_set(0);

    for (int i = 0; i < numButtons; i ++) {
        if (browserState->buttons[i].visible) {
            attron(COLOR_PAIR(1));
            if (browserState->buttons[i].selected) {
                attron(COLOR_PAIR(2));
            }
            mvprintw(
                browserState->buttons[i].y,
                browserState->buttons[i].x,
                browserState->buttons[i].text
            );
            attron(COLOR_PAIR(1));
        }
    }
    for (int i = 0; i < numButtons; i ++) {
        if (browserState->buttons[i].selected) {
            move(
                browserState->buttons[i].y,
                browserState->buttons[i].x
            );
        }
        curs_set(0);
    }

    for (int i = 0; i < numTextAreas; i ++) {
        if (browserState->text_areas[i].visible) {
            attron(COLOR_PAIR(1));
            if (browserState->text_areas[i].selected) {
                attron(COLOR_PAIR(2));
            }

            char *renderResult = getTextAreaRendered(browserState->text_areas[i]);

            mvprintw(
                browserState->text_areas[i].y,
                browserState->text_areas[i].x,
                renderResult
            );
            free(renderResult);
            attron(COLOR_PAIR(1));
        }
    }
    for (int i = 0; i < numTextAreas; i ++) {
        if (browserState->text_areas[i].selected) {
            move(
                browserState->text_areas[i].y,
                browserState->text_areas[i].x + strlen(browserState->text_areas[i].currentText) - browserState->text_areas[i].scrolledAmount
            );
            curs_set(1);
        }
    }
    refresh();
    // `refresh` is automatically called by getch, called after render_nc
}

void initWindow(struct nc_state *state) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_BLACK, COLOR_WHITE);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);
}

void endProgram() {
    endwin();
    exit(0);
}
