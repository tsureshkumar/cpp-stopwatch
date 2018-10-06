#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <thread>
#include <curses.h>
#include <atomic>
#include <vector>


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
    Lap(millitime<millis> st):start(st) {}
};
atomic<int> lap(0); // current lap
atomic<int> page(0); // paging if it goes beyond current screen
vector<Lap> laps;

int width,height; // terminal width, height

class StatusWindow {
    private: WINDOW *w;
    public:
             StatusWindow(int x, int y, int wd, int ht): w(newwin(ht,wd,y,x)) {}
             ~StatusWindow() {if(w) delwin(w);}
             void resize(int x, int y, int width, int height) {
                 wresize(w, width, height);
                 mvwin(w, y, x);
                 wrefresh(w);
             }
             void draw(string x) {
                 mvwprintw(w,0,0, "%s", x.c_str());
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
             TimerWindow(int x, int y, int wd, int ht): w(newwin(ht,wd,y,x)), ht(ht), wd(wd) {}
             ~TimerWindow() { if (w) delwin(w); }
             int curx() {int i=lap%watchesPerPage(); return i%(ht); }
             int cury() {int i=lap%watchesPerPage(); return i/ht*WATCH_WIDTH; }
             int watchesPerPage() {
                 int watchCols = wd/WATCH_WIDTH;
                 return watchCols * ht;
             }
             int pages() {
                 return laps.size()/watchesPerPage()+1;
             }
             void drawTime(int h, int m, int s, int ms) {
                 if (page !=  pages()-1) return;  // if not last page, don't draw current timer
                 drawTime(curx(), cury(), h, m, s, ms);
             }
             void drawTime(int x, int y, int h, int m, int s, int ms) {
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
                     Lap data = laps[i];
                     long ms = (data.fin - data.start).count();
                     int seconds = ms/1000000;
                     int mins = seconds/60;
                     int hours = mins/60;
                     int x = (i-page*watchesPerPage())%(ht);
                     int y = (i-page*watchesPerPage())/ht*WATCH_WIDTH;
                     drawTime(x, y, hours, mins%60, seconds%60, ms%1000);
                 }
             }
};

class PageWindow {
    private: WINDOW *w;
    public:
             PageWindow(int x, int y, int width, int heitht): w(newwin(height, width, y, x)) {}
             ~PageWindow() { if(w) delwin(w); }
             void update() {
                 mvwprintw(w, 0,0,"Page %d", page+0);
                 wrefresh(w);
             }
};

class UI {
    private: StatusWindow *stw; TimerWindow *_timerw; PageWindow *_pagew;
    public:
             UI(int x, int y, int wd, int ht) {
                stw = new StatusWindow(x,y,10,1);
                _timerw = new TimerWindow(x,y+1,wd,ht-1);
                _pagew = new PageWindow(wd-8,y,8,1);
             }
             ~UI () {
                 if(stw) delete stw;
                 if(_timerw) delete _timerw;
             }
             StatusWindow *statusw() { return stw; }
             TimerWindow *timerw() { return _timerw; }
             PageWindow *pagew() { return _pagew; }
};

UI *ui;

int pages; // total number pages
void recalculateSizes() {
    getmaxyx(stdscr, height, width);
}
void stopwatch() {
    //time_t start = time(0);
    laps.emplace_back(chrono::time_point_cast<millis>(sclock::now()));
    while(stopWatchStatus != STOPWATCH_STOPPED) {
        if (stopWatchStatus == STOPWATCH_RUNNING) {
            //long seconds = difftime(time(0), start);
            auto start = laps[lap].start;
            auto now = chrono::system_clock::now();
            long ms = (now - start).count();
            int seconds = ms/1000000;
            int mins = seconds/60;
            int hours = mins/60;
            if(ui) {
                ui->timerw()->drawTime(hours, mins%60, seconds%60, ms%1000);
            }
        }
        this_thread::sleep_for(std::chrono::milliseconds(100));
    }
} 

void cleanup() {
    cout << "cleaning up" << endl;
    endwin();
}

void inputhandler() {
    while(true) {
        int ch = getch();
        if (ch != ERR) {
            if (ch == 27 || ch == 3) { 
                stopWatchStatus = 3;
                break;
            } else if(tolower(ch) == 'p' || ch == ' ') {
                if (stopWatchStatus == STOPWATCH_PAUSED) {
                    stopWatchStatus = STOPWATCH_RUNNING;
                    ui->statusw()->clear();
                } else {
                    stopWatchStatus = STOPWATCH_PAUSED;
                    ui->statusw()->draw("==paused==");
                }
            } else if (tolower(ch)== 'n') {
                laps[lap].fin = chrono::time_point_cast<millis>(sclock::now());
                stopWatchStatus = STOPWATCH_PAUSED;
                laps.emplace_back(chrono::time_point_cast<millis>(sclock::now()));
                lap++;
                if(lap >= ui->timerw()->watchesPerPage()) {
                    page++;
                    ui->timerw()->drawPage();
                }
                stopWatchStatus = STOPWATCH_RUNNING;
            } else if (ch == KEY_NPAGE) {
                page = min(page+1, ui->timerw()->pages()-1);
                ui->timerw()->drawPage();
                ui->pagew()->update();
            } else if (ch == KEY_PPAGE) {
                page = max(page-1, 0);
                ui->timerw()->drawPage();
                ui->pagew()->update();
            }
        }
        //this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main(int argc, char const* argv[])
{
    initscr();
    //auto scr = newterm(nullptr, stdout, stdin);
    nonl(); cbreak(); noecho(); keypad(stdscr, TRUE);
    nodelay(stdscr, false);
    clear();
    curs_set(0);
    recalculateSizes();
    ui = new UI(0,0,width, height);
    thread t(stopwatch);
    thread keyhandler(inputhandler);
    t.join();
    keyhandler.join();
    delete ui;
    cleanup();
    //delscreen(scr);
    return 0;
}

