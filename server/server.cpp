#include <iostream>
#include <cassert>
#include <fstream>
#include <string>
#include <list>
#include <sstream>
#include <iomanip>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <cstring>

#include "dijkstra.h"

struct Point {
    long long lat, lon;
};

// return the manhattan distance between the two points
long long manhattan(const Point& pt1, const Point& pt2) {
  long long dLat = pt1.lat - pt2.lat, dLon = pt1.lon - pt2.lon;
  return abs(dLat) + abs(dLon);
}

// find the ID of the point that is closest to the given point "pt"
int findClosest(const Point& pt, const unordered_map<int, Point>& points) {
  pair<int, Point> best = *points.begin();

  for (const auto& check : points) {
    if (manhattan(pt, check.second) < manhattan(pt, best.second)) {
      best = check;
    }
  }
  return best.first;
}

// read the graph from the file that has the same format as the "Edmonton graph" file
void readGraph(const string& filename, WDigraph& g, unordered_map<int, Point>& points) {
  ifstream fin(filename);
  string line;

  while (getline(fin, line)) {
    // split the string around the commas, there will be 4 substrings either way
    string p[4];
    int at = 0;
    for (auto c : line) {
      if (c == ',') {
        // start new string
        ++at;
      }
      else {
        // append character to the string we are building
        p[at] += c;
      }
    }

    if (at != 3) {
      // empty line
      break;
    }

    if (p[0] == "V") {
      // new Point
      int id = stoi(p[1]);
      assert(id == stoll(p[1])); // sanity check: asserts if some id is not 32-bit
      points[id].lat = static_cast<long long>(stod(p[2])*100000);
      points[id].lon = static_cast<long long>(stod(p[3])*100000);
      g.addVertex(id);
    }
    else {
      // new directed edge
      int u = stoi(p[1]), v = stoi(p[2]);
      g.addEdge(u, v, manhattan(points[u], points[v]));
    }
  }
}

int create_and_open_fifo(const char * pname, int mode) {
  // create a fifo special file in the current working directory with
  // read-write permissions for communication with the plotter app
  // both proecsses must open the fifo before they perform I/O operations
  // Note: pipe can't be created in a directory shared between 
  // the host OS and VM. Move your code outside the shared directory
  if (mkfifo(pname, 0666) == -1) {
    cout << "Unable to make a fifo. Make sure the pipe does not exist already!" << endl;
    cout << errno << ": " << strerror(errno) << endl << flush;
    exit(-1);
  }

  // opening the fifo for read-only or write-only access
  // a file descriptor that refers to the open file description is
  // returned
  int fd = open(pname, mode);

  if (fd == -1) {
    cout << "Error: failed on opening named pipe." << endl;
    cout << errno << ": " << strerror(errno) << endl << flush;
    exit(-1);
  }

  return fd;
}

// keep in mind that in part 1, the program should only handle 1 request
// in part 2, you need to listen for a new request the moment you are done
// handling one request
int main() {
  WDigraph graph;
  unordered_map<int, Point> points;

  const char *inpipe = "inpipe";
  const char *outpipe = "outpipe";

  // Open the two pipes
  int in = create_and_open_fifo(inpipe, O_RDONLY);
  cout << "inpipe opened..." << endl;
  int out = create_and_open_fifo(outpipe, O_WRONLY);
  cout << "outpipe opened..." << endl;  

  // build the graph
  readGraph("server/edmonton-roads-2.0.1.txt", graph, points);

  // NOTE: in Part II you will use a different communication protocol than Part I
  // So edit the code below to implement this protocol

  // initialization; prevent memory leaks
  int cur = 0;
  Point s, e;
  char endprint[3] = "E\n";
  
  // while the pipes are open...
  while (in != -1 and out != -1) {

    // we will create block buffers for the read/write
    char *inbuffer = new char[43];
    char *outbuffer = new char[22];

    // read in the coordinates; will always be 43 characters in length
    read(in, inbuffer, 43);

    // checking valid input; input will always start with 5 or Q
    if (*inbuffer == '5' or *inbuffer == 'Q') {

      // to store the split coordinates
      string pointString[4];
      cur = 0;
      
      // this is just the readfile code modified to use the buffer
      for (long unsigned int i{0}; i < 43; i++) {
        if (*(inbuffer+i) == ' ' or *(inbuffer+i) == '\n') {
          // start new string if we hit delimiter
          ++cur;

          // added a quit function for input
        } else if (*(inbuffer+i) == 'Q') {
          delete[] inbuffer;
          delete[] outbuffer;
          // flag jumping; easier to handle than multi-breaks
          goto quit;
        } else {
          // append character to the string we are building
          pointString[cur] += *(inbuffer+i);
        }
      }
      
      // assigning the coordinates to points and finding closest ones
      s.lat = static_cast<long long>(stod(pointString[0])*100000);
      s.lon = static_cast<long long>(stod(pointString[1])*100000);
      e.lat = static_cast<long long>(stod(pointString[2])*100000);
      e.lon = static_cast<long long>(stod(pointString[3])*100000);
      int start = findClosest(s, points), end = findClosest(e, points);

      // doing dijkstra algorithm
      unordered_map<int, PIL> tree;
      dijkstra(graph, start, tree);

      // if there is a path:
      if (tree.find(end) != tree.end()) {
        // read off the path by stepping back through the search tree
        list<int> path;
        while (end != start) {
          path.push_front(end);
          end = tree[end].first;
        }
        path.push_front(start);

        // using temporary stream and stream formatting to write
        // the proper format to the outpipe
        for (int v : path) {
          stringstream tempstream;
          tempstream << fixed;
          tempstream << setprecision(6);
          tempstream << static_cast<double>(points[v].lat)/100000 << ' ' << static_cast<double>(points[v].lon)/100000 << endl;
          strcpy(outbuffer, tempstream.str().c_str());
          write(out, outbuffer, 22);
        }
      }
      // end of coordinates, print 'E' followed by newline
      strcpy(outbuffer,endprint);
      write(out, outbuffer, 2);
    }
    // freeing memory
    delete[] inbuffer;
    delete[] outbuffer;
  }

  // that flag we used earlier
  quit:

  //deleting pipe files once we are finished
  remove(inpipe);
  remove(outpipe);
  return 0;
}
