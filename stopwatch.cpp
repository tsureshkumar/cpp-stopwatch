#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <thread>
#include <curses.h>
#include <atomic>


using namespace std;
atomic<int> stopWatchStatus(1);

void stopwatch() {
    //time_t start = time(0);
    auto start = chrono::time_point_cast<chrono::milliseconds>(chrono::system_clock::now());
    while(stopWatchStatus != 3) {
        if (stopWatchStatus == 1) {
            //long seconds = difftime(time(0), start);
            auto now = chrono::system_clock::now();
            long ms = (now - start).count();
            int seconds = ms/1000000;
            int mins = seconds/60;
            int hours = mins/60;
            stringstream ss;
            ss << setfill('0')  << setw(2) << hours << ":";
            ss << setfill('0')  << setw(2) << mins%60 << ":";
            ss << setfill('0')  << setw(2) << seconds%60 << " ";
            ss << setfill('0')  << setw(3) << ms%1000;
            mvprintw(1,1,"%s", ss.str().c_str());
            refresh();
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
            } else if(ch == 'p' || ch == ' ') {
                if (stopWatchStatus == 2) {
                    move(2,0); clrtoeol();
                    stopWatchStatus = 1;
                    //mvprintw(1,0, "%s", "==unpaused==");
                } else {
                    stopWatchStatus = 2;
                    mvprintw(2,0, "%s", "==paused==");
                }
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
    thread t(stopwatch);
    thread keyhandler(inputhandler);
    t.join();
    keyhandler.join();
    cleanup();
    //delscreen(scr);
    return 0;
}

