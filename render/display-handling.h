#ifndef _DSP_HANDLING
    #define _DSP_HANDLING 1

#include <ncurses.h>

#include "../utils/string.h"

typedef void (*clickHandler)(void *, char *);

enum nc_page {
    PAGE_DOCUMENT_LOADED,
    PAGE_GOTO_DIALOG,
    PAGE_GOTO_OVER_DOCUMENT,
    PAGE_VIEW_COOKIES,
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
    int allocatedTextLen;
    int currentTextLength;

    char *descriptor;
    int selected;
    int scrolledAmount;
    int allowMoreChars;
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

    int overrideMinX;
};

struct nc_text {
    int y;
    int x;
    int visible;
    char *text;
    char *descriptor;
};

struct nc_selected {
    char *textarea;
    char *button;
};

struct nc_selected selectableFromButton(struct nc_button *button) {
    struct nc_selected select;
    select.button = button->descriptor;
    select.textarea = NULL;
    return select;
}

struct nc_selected selectableFromTextarea(struct nc_text_area *textarea) {
    struct nc_selected select;
    select.button = NULL;
    select.textarea = textarea->descriptor;
    return select;
}

struct nc_state {
    enum nc_page currentPage;

    struct nc_text_area text_areas[256];
    struct nc_button *buttons;
    struct nc_text texts[256];
    int numTextAreas;
    int numButtons;
    int numTexts;

    int initialButtons; // Number of buttons right after initialization
    int globalScrollX;
    int globalScrollY;
    int shouldCheckAutoScroll;

    struct nc_selected *selectables;
    int numSelectables;
    int selectableIndex;

    char *currentPageUrl;

    // todo: add a forward/back buffer
};

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

int *getYPosOfCurrentSelected(struct nc_state *state) {
    int *data = (int *) calloc(1, sizeof(int));
    for (int i = 0; i < state->numButtons; i ++) {
        if (state->buttons[i].selected && state->buttons[i].visible && state->buttons[i].enabled) {
            data[0] = state->buttons[i].y;
            return data;
        }
    }
    for (int i = 0; i < state->numTextAreas; i ++) {
        if (state->text_areas[i].selected && state->text_areas[i].visible) {
            data[0] = state->text_areas[i].y;
            return data;
        }
    }
    free(data);
    return NULL;
}

int isSelectable(struct nc_state *state, struct nc_selected *selectable) {
    if (selectable->button != NULL) {
        struct nc_button *btn = getButtonByDescriptor(state, selectable->button);
        return btn->visible && btn->enabled;
    } else if (selectable->textarea != NULL) {
        struct nc_text_area *txtar = getTextAreaByDescriptor(state, selectable->textarea);
        return txtar->visible;
    } else {
        // Selectable was invalidated because the button/textarea it contained was removed
        return 0;
    }
}

void freeButtons(struct nc_state *state) {
    for (int i = state->numSelectables - 1; i >= 0; i --) {
        struct nc_selected *selectable = &state->selectables[i];
        // this is a little silly
        if (selectable->button) {
            if (!strncmp(selectable->button, "_temp_", 6) && getButtonByDescriptor(state, selectable->button)->descriptor != NULL) {
                selectable->button = NULL;
            }
        } else if (selectable->textarea) {
            if (!strncmp(selectable->textarea, "_temp_", 6) && getTextAreaByDescriptor(state, selectable->textarea)->descriptor != NULL) {
                selectable->textarea = NULL;
            }
        }
    }
    for (int i = state->numButtons - 1; i >= state->initialButtons; i --) {
        state->buttons[i].descriptor = NULL;
        state->buttons[i].text = NULL;
        state->buttons[i].visible = 0;
        state->buttons[i].enabled = 0;
        state->numButtons --;
    }
    state->buttons = realloc(state->buttons, state->initialButtons * sizeof(struct nc_button));
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
    button.text = nc_strcpy(text);
    button.onclick = onclick;
    button.overrideMinX = -1;
    button.descriptor = nc_strcpy(descriptor);

    state->numButtons ++;
    state->buttons = realloc(state->buttons, state->numButtons * sizeof(struct nc_button));
    state->buttons[state->numButtons - 1] = button;

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
    textarea.allocatedTextLen = 1024;
    textarea.descriptor = nc_strcpy(descriptor);
    textarea.scrolledAmount = 0;
    textarea.allowMoreChars = 0;

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

int getTextAreaWidth(struct nc_text_area textarea) {
    int mx;
    int my;
    getmaxyx(stdscr, my, mx);

    return (textarea.width < 0) ? (mx + textarea.width) : textarea.width;
}

int getButtonX(struct nc_button button) {
    int mx;
    int my;
    getmaxyx(stdscr, my, mx);

    return (button.x < 0) ? (mx + button.x) : button.x;
}

char *getTextAreaRendered(struct nc_text_area textarea) {
    int width = getTextAreaWidth(textarea);

    char *resultingText = (char *) calloc(width + 2, sizeof(char));

    int under = width - strlen(textarea.currentText);
    if (under < 0) under = 0;
    for (int i = textarea.scrolledAmount; i < strlen(textarea.currentText); i ++) {
        resultingText[i - textarea.scrolledAmount] = textarea.currentText[i];
    }
    for (int i = 0; i < under; i ++) {
        resultingText[strlen(resultingText)] = '_';
    }

    return resultingText;
}

void setTextOf(struct nc_text_area *textarea, char *newText) {
    int len = strlen(newText);
    if (len > getTextAreaWidth(*textarea)) {
        textarea->scrolledAmount = len - getTextAreaWidth(*textarea);
    } else {
        textarea->scrolledAmount = 0;
    }

    textarea->currentText = realloc(textarea->currentText, len + 2);
    textarea->allocatedTextLen = len + 2;
    textarea->currentTextLength = len;

    strcpy(textarea->currentText, newText);
}

void appendCharTo(struct nc_text_area *textarea, char newChar) {
    textarea->allocatedTextLen ++;
    textarea->currentText = realloc(textarea->currentText, textarea->allocatedTextLen);
    textarea->currentText[textarea->currentTextLength] = newChar;

    textarea->currentTextLength ++;
    textarea->currentText[textarea->currentTextLength] = 0;

    int len = strlen(textarea->currentText);
    if (len > getTextAreaWidth(*textarea)) {
        textarea->scrolledAmount = len - getTextAreaWidth(*textarea);
    } else {
        textarea->scrolledAmount = 0;
    }
}

void popCharFrom(struct nc_text_area *textarea) {
    if (textarea->currentText[0] == 0) return;

    textarea->allocatedTextLen --;
    textarea->currentTextLength --;
    textarea->currentText[textarea->currentTextLength] = '\0';
    textarea->currentText = realloc(textarea->currentText, textarea->allocatedTextLen);

    int len = strlen(textarea->currentText);
    if (len > getTextAreaWidth(*textarea)) {
        textarea->scrolledAmount = len - getTextAreaWidth(*textarea);
    } else {
        textarea->scrolledAmount = 0;
    }
}

void clearCharsOf(struct nc_text_area *textarea) {
    memset(textarea->currentText, 0, textarea->allocatedTextLen);
    textarea->scrolledAmount = 0;
    textarea->currentTextLength = 0;
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
    if (curSelected.textarea != NULL) {
        struct nc_text_area *textarea = getTextAreaByDescriptor(state, curSelected.textarea);
        textarea->selected = 1;
    } else if (curSelected.button != NULL) {
        struct nc_button *button = getButtonByDescriptor(state, curSelected.button);
        button->selected = 1;
    } else {
        // neither are selected, this is an "empty" selectable
    }

    if (state->shouldCheckAutoScroll) {
        int mx;
        int my;
        getmaxyx(stdscr, my, mx);

        int *yposp = getYPosOfCurrentSelected(state);
        if (yposp != NULL) {
            int ypos = (*yposp) + state->globalScrollY;
            if (ypos < 0) {
                state->globalScrollY = -(ypos - state->globalScrollY);
            }
            if (ypos > my - 1) {
                state->globalScrollY = (-(ypos - state->globalScrollY) + my) - 1;
            }
            free(yposp);
        }
    }
}

char *repeatChar(char c, int times) {
    char *mem = (char *) calloc(times + 1, sizeof(char));
    for (int i = 0; i < times; i ++) {
        mem[i] = c;
    }
    return mem;
}

void nc_templinkpresshandler(void *state, char *url) {}

void nc_noopbuttonhandler(void *state, char *url) {}

void printText(struct nc_state *state, int y, int x, char *text, int invertColors, int overrideMinX, int hasDynamicButtons) {
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

    char *fullDashes = repeatChar('-', mx);
    char *fullHorizontalRow = (char *) calloc(mx + 1, sizeof(char));
    strcpy(fullHorizontalRow, fullDashes);
    free(fullDashes);

    if (overrideMinX != -1) {
        minX = overrideMinX;
    }

    int escapeModeEnabled = 0;
    int italics = 0;
    int bold = 0;
    int underline = 0;
    int strikethrough = 0;
    int doIndent = 0;
    int aboutToCreateButton = 0;
    int colorsP[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int colorStackNum = 0;
    char clickableStack[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int clickableStackNum = 0;
    unsigned int linkIdx1 = 0;
    unsigned int linkIdx2 = 1;
    int tooManyLinks = 0;
    char *buttonSpace = (char *) calloc(len + 1, sizeof(char));
    int posx = 0;
    int posy = 0;
    for (int i = 0; i < len; i ++) {
        // This should be run no matter what
        if (aboutToCreateButton) {
            buttonSpace[strlen(buttonSpace)] = text[i];
        }

        if (text[i] == '\\' && !escapeModeEnabled) {
            escapeModeEnabled = 1;
            continue;
        }
        if (escapeModeEnabled) {
            int doContinue = 0;
            switch (text[i]) {
                case 'h':
                    mvaddstr(realPosY + state->globalScrollY, realPosX + state->globalScrollX, horizontalRow);
                    realPosX = mx;
                    break;
                case 'H':
                    mvaddstr(realPosY + state->globalScrollY, realPosX + state->globalScrollX, fullHorizontalRow);
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

                case 'd':
                    clickableStack[clickableStackNum] = 1;
                    clickableStackNum ++;
                    if (clickableStackNum > 62) {
                        // This limit should (hopefully) never be reached, but let's just make sure.
                        clickableStackNum --;
                    }
                    break;
                case 'e':
                    clickableStack[clickableStackNum] = 0;
                    clickableStackNum ++;
                    if (clickableStackNum > 62) {
                        // This limit should (hopefully) never be reached, but let's just make sure.
                        clickableStackNum --;
                    }
                    break;
                case 'f':
                    if (clickableStackNum > 0) {
                        clickableStackNum --;
                        clickableStack[clickableStackNum] = 0;
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

                case 'm':
                    aboutToCreateButton = 1;
                    posx = realPosX;
                    posy = realPosY;
                    break;
                case 'n':
                    if (linkIdx1 == 255) {
                        linkIdx2 ++;
                        linkIdx1 = 0;
                    }
                    if (linkIdx2 == 6 && !tooManyLinks) {
                        tooManyLinks = 1;
                    }
                    linkIdx1 ++;
                    if (tooManyLinks) {
                        break;
                    }
                    int buttonSpaceLen = strlen(buttonSpace) - 1;
                    switch(text[i + 1]) {
                        case 'L': // Link
                            aboutToCreateButton = 0;
                            buttonSpace[buttonSpaceLen] = 0; // remove \ 
                            buttonSpace[buttonSpaceLen - 1] = 0; // remove n
                            char *addr = text + i + 2;
                            char *result = safeDecodeString(addr);

                            char *linkDescriptor = (char *) calloc(strlen(result) + 20, sizeof(char));
                            strcpy(linkDescriptor, "_temp_linkto_");
                            linkDescriptor[strlen(linkDescriptor)] = linkIdx1;
                            linkDescriptor[strlen(linkDescriptor)] = linkIdx2;
                            strcat(linkDescriptor, result);
                            if (!clickableStack[clickableStackNum - 1]) {
                                if (getButtonByDescriptor(state, linkDescriptor)) {
                                    // Already exists, don't create again, but do update position
                                    struct nc_button *linkButton = getButtonByDescriptor(state, linkDescriptor);
                                    linkButton->x = posx;
                                    linkButton->y = posy;
                                    free(linkDescriptor);
                                } else {
                                    createNewButton(state, posx, posy, makeStrCpy(buttonSpace), nc_templinkpresshandler, linkDescriptor);
                                    struct nc_button *linkButton = getButtonByDescriptor(state, linkDescriptor);
                                    linkButton->overrideMinX = minX;
                                }
                            }
                            free(result);
                            clrStr(buttonSpace);
                            i += safeGetEncodedStringLength(addr) + 1;
                            break;
                        case 'N': // NO-OP
                            aboutToCreateButton = 0;
                            buttonSpace[buttonSpaceLen] = 0; // remove \ 
                            buttonSpace[buttonSpaceLen - 1] = 0; // remove n
                            char *noopDescriptor = (char *) calloc(32, sizeof(char));
                            strcpy(noopDescriptor, "_temp_noop_");
                            noopDescriptor[strlen(noopDescriptor)] = linkIdx1;
                            noopDescriptor[strlen(noopDescriptor)] = linkIdx2;
                            strcat(noopDescriptor, "NOOP");
                            if (!clickableStack[clickableStackNum - 1]) {
                                if (getButtonByDescriptor(state, noopDescriptor)) {
                                    // Already exists, don't create again, but do update position
                                    struct nc_button *noopButton = getButtonByDescriptor(state, noopDescriptor);
                                    noopButton->x = posx;
                                    noopButton->y = posy;
                                    free(noopDescriptor);
                                } else {
                                    createNewButton(state, posx, posy, makeStrCpy(buttonSpace), nc_noopbuttonhandler, noopDescriptor);
                                    struct nc_button *linkButton = getButtonByDescriptor(state, noopDescriptor);
                                    linkButton->overrideMinX = minX;
                                }
                            }
                            clrStr(buttonSpace);
                            i += 1;
                            break;
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

                case 'y':
                    strikethrough ++;
                    if (strikethrough > 1048576) {
                        // This limit should never be reached, but let's just make sure.
                        strikethrough --;
                    }
                    break;
                case 'z':
                    if (strikethrough > 0) {
                        strikethrough --;
                    }
                    break;

                case '1':
                    colorsP[colorStackNum] = 5;
                    colorStackNum ++;
                    if (colorStackNum > 62) {
                        // This limit should (hopefully) never be reached, but let's just make sure.
                        colorStackNum --;
                    }
                    break;
                case '2':
                    colorsP[colorStackNum] = 4;
                    colorStackNum ++;
                    if (colorStackNum > 62) {
                        // This limit should (hopefully) never be reached, but let's just make sure.
                        colorStackNum --;
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
                case '9':
                    colorsP[colorStackNum] = 9;
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
        if (realPosY + state->globalScrollY < my) {
            if (realPosY + state->globalScrollY >= 0 && realPosX + state->globalScrollX >= 0) {
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

                if (strikethrough) {
                    // No ncurses support for strikethrough
                }

                if (invertColors) {
                    if (colorStackNum) {
                        attron(COLOR_PAIR(256 - colorsP[colorStackNum - 1]));
                    } else {
                        attron(COLOR_PAIR(255));
                    }
                } else {
                    if (colorStackNum) {
                        attron(COLOR_PAIR(colorsP[colorStackNum - 1]));
                    } else {
                        attron(COLOR_PAIR(1));
                    }
                }

                mvaddch(realPosY + state->globalScrollY, realPosX + state->globalScrollX, toPrint);
            }
        } else if (!hasDynamicButtons) {
            break;
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
    free(buttonSpace);
    free(fullHorizontalRow);
}

void render_nc(struct nc_state *browserState) {
    clear();
    int numTexts = browserState->numTexts;
    for (int i = 0; i < numTexts; i ++) {
        if (browserState->texts[i].visible) {
            printText(browserState, browserState->texts[i].y, browserState->texts[i].x, browserState->texts[i].text, 0, -1, 1);
        }
    }
    curs_set(0);

    int numButtons = browserState->numButtons;
    for (int i = 0; i < numButtons; i ++) {
        if (browserState->buttons[i].visible) {
            printText(
                browserState,
                browserState->buttons[i].y,
                getButtonX(browserState->buttons[i]),
                browserState->buttons[i].text,
                browserState->buttons[i].selected,
                browserState->buttons[i].overrideMinX,
                0
            );
        }
    }

    attron(COLOR_PAIR(1));

    int numTextAreas = browserState->numTextAreas;
    for (int i = 0; i < numTextAreas; i ++) {
        if (browserState->text_areas[i].visible) {
            attron(COLOR_PAIR(1));
            if (browserState->text_areas[i].selected) {
                attron(COLOR_PAIR(255));
            }

            char *renderResult = getTextAreaRendered(browserState->text_areas[i]);

            mvprintw(
                browserState->text_areas[i].y + browserState->globalScrollY,
                browserState->text_areas[i].x + browserState->globalScrollX,
                renderResult
            );
            free(renderResult);
            attron(COLOR_PAIR(1));
        }
    }
    int my;
    int mx;
    getmaxyx(stdscr, my, mx);
    for (int i = 0; i < numTextAreas; i ++) {
        if (browserState->text_areas[i].selected) {
            int ypos = browserState->text_areas[i].y + browserState->globalScrollY;
            if (ypos < 0 || ypos > my - 1) {
                curs_set(0);
            } else {
                move(
                    ypos,
                    browserState->text_areas[i].x + browserState->globalScrollX + strlen(browserState->text_areas[i].currentText) - browserState->text_areas[i].scrolledAmount
                );
                curs_set(1);
            }
        }
    }

    for (int i = 0; i < numButtons; i ++) {
        if (browserState->buttons[i].selected) {
            move(
                browserState->buttons[i].y + browserState->globalScrollY,
                getButtonX(browserState->buttons[i]) + browserState->globalScrollX
            );
        }
    }

    refresh();
    // `refresh` is automatically called by getch, called after render_nc
}

void createColorPair(char num, int fgColor, int bgColor) {
    init_pair(num, fgColor, bgColor);
    init_pair(256 - num, bgColor, fgColor);
}

void createColor(char num, int r, int g, int b) {
    init_color(num, r * (1000/255), g * (1000/255), b * (1000/255));
}

void initWindow() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    createColorPair(1, COLOR_WHITE, COLOR_BLACK);

    createColorPair(3, COLOR_BLUE, COLOR_BLACK);
    createColorPair(4, COLOR_GREEN, COLOR_BLACK);
    createColorPair(5, COLOR_RED, COLOR_BLACK);

    // Avoid overriding color #8, it's the default color for non-renderable unicode
    createColor(9, 0xff, 0xe8, 0xbd); // wheat
    createColorPair(9, 9, COLOR_BLACK);
}

void nc_onInitFinish(struct nc_state *state) {
    state->initialButtons = state->numButtons;
}

void endProgram() {
    endwin();
    exit(0);
}

#endif
