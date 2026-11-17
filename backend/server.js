// Simple backend: receives requests from the browser and runs the C program.
// Uses only Node's built-in modules, so no "npm install" is needed.

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { refreshStorage } = require('./storage');

const PORT = 3000;
const EXE = path.join(__dirname, process.platform === 'win32' ? 'booking.exe' : 'booking');
const FRONTEND = path.join(__dirname, '..', 'frontend');
const WRITING_ACTIONS = new Set(['addResource', 'sortResources', 'bookResource', 'cancelBooking']);

// For each action: the answers the C program will ask for, in the SAME order as its questions
const ACTIONS = {
  login:             ['password'],
  addResource:       ['id', 'name', 'type', 'capacity'],
  viewResources:     [],
  searchResource:    ['id'],
  sortResources:     [],
  bookResource:      ['resId', 'date', 'slot', 'name', 'joinWaiting'],
  viewBookings:      [],
  cancelBooking:     ['id'],
  viewWaitingList:   [],
  bookingHistory:    [],
  bookingStatistics: []
};

// Only these frontend files can be downloaded
const PAGES = {
  '/':           ['index.html', 'text/html'],
  '/index.html': ['index.html', 'text/html'],
  '/style.css':  ['style.css',  'text/css'],
  '/script.js':  ['script.js',  'text/javascript']
};

// Run the C program once, give it the answers on stdin, return everything it prints.
// Requests wait in a queue so two requests never write the .txt files at the same time.
let queue = Promise.resolve();
function runC(role, action, answers) {
  const job = queue.then(() => new Promise((resolve) => {
    const child = spawn(EXE, ['--api', role, action], { cwd: __dirname }); // cwd = backend folder, where the .txt files are
    let output = '';
    child.stdout.on('data', (d) => (output += d));
    child.stdin.on('error', () => {});
    child.on('error', () => resolve('Cannot start the C program. Did you compile it? (see README steps)'));
    child.on('close', (code) => {
      if (code === 0 && WRITING_ACTIONS.has(action) && !output.includes('Warning: cannot write ')) {
        refreshStorage(__dirname);
      }
      resolve(output);
    });
    child.stdin.write(answers.join('\n') + '\n');
    child.stdin.end();
  }));
  queue = job;
  return job;
}

const server = http.createServer((req, res) => {
  // 1) API request from the browser:  POST /api/<action>
  if (req.method === 'POST' && req.url.startsWith('/api/')) {
    const action = req.url.slice(5);
    let body = '';
    req.on('data', (chunk) => (body += chunk));
    req.on('end', async () => {
      let data = {};
      try { data = JSON.parse(body); } catch (e) {}
      const role = String(data.role);
      let output;
      if (!ACTIONS[action] || !/^[123]$/.test(role)) {
        output = 'Bad request.';
      } else {
        // take the needed fields from the request; remove line breaks so one field = one line
        const answers = ACTIONS[action].map((f) => String(data[f] ?? '').replace(/[\r\n]/g, ' '));
        output = await runC(role, action, answers);
      }
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ output }));
    });
    return;
  }

  // 2) Otherwise send a frontend file
  const page = PAGES[req.url];
  if (page) {
    res.writeHead(200, { 'Content-Type': page[1] });
    res.end(fs.readFileSync(path.join(FRONTEND, page[0])));
  } else {
    res.writeHead(404);
    res.end('Not found');
  }
});

refreshStorage(__dirname);
server.listen(PORT, () => console.log('Open http://localhost:' + PORT));
