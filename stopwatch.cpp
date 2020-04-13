#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <thread>
#include <curses.h>
#include <atomic>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif // __EMSCRIPTEN__


using namespace std;
using sclock = std::chrono::system_clock;
using millis = std::chrono::milliseconds;
template<class Duration>
using millitime = std::chrono::time_point<sclock, Duration>;

const int WATCH_WIDTH=13;
enum STOPWATCH_STATUS {
    STOPWATCH_RUNNING = 1,   
    STOPWATCH_PAUSED,
    STOPWATCH_STOPPED
};
atomic<int> stopWatchStatus(STOPWATCH_RUNNING);

struct Lap {
    millitime<millis> start,fin;
    atomic<long> counter; // milliseconds past start
    Lap(millitime<millis> st):start(st), counter(0) {}
    Lap(const Lap &other) {
        start = other.start;
        fin = other.fin;
        counter.store(other.counter);
    }
};
atomic<int> lap(0); // current lap
atomic<int> page(0); // paging if it goes beyond current screen
vector<Lap> laps;

int width,height; // terminal width, height

class StatusWindow {
    private: WINDOW *w; string status;
    public:
             StatusWindow(int x, int y, int wd, int ht): w(newwin(ht,wd,y,x)) { box(w,0,0);}
             ~StatusWindow() {if(w) delwin(w);}
             void draw(string x) {
                 status = x;
             }
             void render() {
                 if (stopWatchStatus == STOPWATCH_PAUSED) {
                     mvwprintw(w,1,1, "=== paused === ");
                 } else
                     mvwprintw(w,1,1, "               ");
                 mvwprintw(w,2,1, "[p/SPC] pause [q/ESC] quit [n] next lap [[/]] page next/prev");
                 wrefresh(w);
             }
             void clear() {
                 werase(w);
                 wrefresh(w);
             }
};

class TimerWindow {
    private: WINDOW *w; int wd, ht;
    public:
             TimerWindow(int x, int y, int wd, int ht):  ht(ht-2), wd(wd-2), w(newwin(ht,wd,y,x)) {box(w, 0,0);}
             ~TimerWindow() { if (w) delwin(w); }
             int curx() {int i=lap%watchesPerPage(); return i%(ht); }
             int cury() {int i=lap%watchesPerPage(); return i/ht*WATCH_WIDTH; }
             int watchesPerPage() {
                 return wd/WATCH_WIDTH * ht;
             }
             int pages() {
                 return laps.size()/watchesPerPage()+1;
             }
             void drawTime(int h, int m, int s, int ms) {
                 if (page !=  pages()-1) return;  // if not last page, don't draw current timer
                 drawTime(curx(), cury()+1, h, m, s, ms);
             }
             void drawTime(int x, int y, int h, int m, int s, int ms) {
                 box(w,0,0);
                 stringstream ss;
                 ss << setfill('0')  << setw(2) << h << ":";
                 ss << setfill('0')  << setw(2) << m << ":";
                 ss << setfill('0')  << setw(2) << s << " ";
                 ss << setfill('0')  << setw(3) << ms;
                 attron(A_BOLD);
                 mvwprintw(w,x,y,"%s", ss.str().c_str());
                 attroff(A_BOLD);
                 wrefresh(w);
             }
             
             void drawPage() {
                 werase(w);
                 for(int i=page*watchesPerPage(); i < laps.size() && i < (page+1)*watchesPerPage(); i++) {
                     Lap &data = laps[i];
                     long ms = data.counter;
                     int seconds = ms/1000L;
                     int mins = seconds/60;
                     int hours = mins/60;
                     int x = (i-page*watchesPerPage())%(ht);
                     int y = (i-page*watchesPerPage())/ht*WATCH_WIDTH;
                     drawTime(x+1, y+1, hours, mins%60, seconds%60, ms%1000);
                 }
             }
};

class PageWindow {
    private: WINDOW *w;
    public:
             PageWindow(int x, int y, int wd, int ht): w(newwin(ht, wd, y, x)) {box(w,0,0);}
             ~PageWindow() { if(w) delwin(w); }
             void render() {
                 mvwprintw(w, 1,1,"Page %d", page+1);
                 wrefresh(w);
             }
};

class UI {
    private: StatusWindow *stw; TimerWindow *_timerw; PageWindow *_pagew;
    public:
             UI(int x, int y, int wd, int ht) {
                stw = new StatusWindow(x,y,wd,4);
                _timerw = new TimerWindow(x,y+4,wd,ht-7);
                _pagew = new PageWindow(x,ht-3,wd,3);
             }
             ~UI () {
                 if(stw) delete stw;
                 if(_timerw) delete _timerw;
                 if(_pagew) delete _pagew;
             }
             StatusWindow *statusw() { return stw; }
             TimerWindow *timerw() { return _timerw; }
             PageWindow *pagew() { return _pagew; }
             void render() {
                 this->statusw()->render();
                this->timerw()->drawPage();
                this->pagew()->render();
             }
};

UI *ui;

int pages; // total number pages
void recalculateSizes() {
    getmaxyx(stdscr, height, width );
}
void stopwatch() {
    laps.emplace_back(chrono::time_point_cast<millis>(sclock::now()));
    const int mills = 100;
    while(stopWatchStatus != STOPWATCH_STOPPED) {
        if (stopWatchStatus == STOPWATCH_RUNNING) {
            laps[lap].counter += mills;
        }
        this_thread::sleep_for(std::chrono::milliseconds(mills));
    }
} 

void cleanup() {
    cout << "cleaning up" << endl;
    endwin();
}

void inputhandler(int ch) {
    if(tolower(ch) == 'p' || ch == ' ') {
        if (stopWatchStatus == STOPWATCH_PAUSED) 
            stopWatchStatus = STOPWATCH_RUNNING;
        else stopWatchStatus = STOPWATCH_PAUSED;
    } else if (tolower(ch)== 'n') {
        laps[lap].fin = chrono::time_point_cast<millis>(sclock::now());
        stopWatchStatus = STOPWATCH_PAUSED;
        laps.emplace_back(chrono::time_point_cast<millis>(sclock::now()));
        lap++;
        if(lap % ui->timerw()->watchesPerPage() == 0) {
            page++;
        }
        stopWatchStatus = STOPWATCH_RUNNING;
    } else if (ch == KEY_NPAGE || ch == ']') {
        page = min(page+1, ui->timerw()->pages()-1);
    } else if (ch == KEY_PPAGE || ch == '[') {
        page = max(page-1, 0);
    }
}

bool iteration(UI *ui) {
    int ch;
    if((ch = getch()) != 'q' && ch != 'Q' && ch != 3 && ch != 27) {
        if(ch != ERR)
            inputhandler(ch);
        flushinp();
        ui->render();
        refresh();
        return true;
    }
    return false;
}

int main(int argc, char const* argv[])
{
    initscr();
    //auto scr = newterm(nullptr, stdout, stdin);
    nonl(); cbreak(); noecho(); keypad(stdscr, TRUE);
    clear();
    curs_set(0);

    recalculateSizes();
    ui = new UI(0,0, width, height);
    thread t(stopwatch); // thread updating stopwatch

    // main event loop
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([]() {iteration(ui);}, 20, 1);
#else
    struct timespec wait, start,cur;
    wait.tv_sec = 0;
    timeout(0);
    while(true) {
        clock_gettime(CLOCK_REALTIME, &start);
        if(!iteration(ui)) break;

        clock_gettime(CLOCK_REALTIME, &cur);
        wait.tv_nsec = start.tv_sec * 1e9L + start.tv_nsec + 1e9L/20 - (cur.tv_sec * 1e9L + cur.tv_nsec);
        nanosleep(&wait, NULL);
    }
#endif // __EMSCRIPTEN__
    stopWatchStatus = STOPWATCH_STOPPED;
    t.join();
    delete ui;
    cleanup();
    //delscreen(scr);
    return 0;
}

