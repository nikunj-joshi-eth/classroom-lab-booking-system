// Simple backend: receives requests from the browser and runs the C program.
// Uses only Node's built-in modules, so no "npm install" is needed.

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');
const { randomUUID } = require('node:crypto');
const { readAvailability, refreshStorage, validDate } = require('./storage');

const PORT = 3000;
const EXE = path.join(__dirname, process.platform === 'win32' ? 'booking.exe' : 'booking');
const FRONTEND = path.join(__dirname, '..', 'frontend');
const sessions = new Map();
const SESSION_SECONDS = 3600;
const MAX_BODY_BYTES = 8192;

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
  bookingStatistics: [],
  availability:      ['date']
};
const WRITE_ROLES = {
  addResource: ['1'],
  sortResources: ['1'],
  bookResource: ['1', '2'],
  cancelBooking: ['1', '2']
};

const isInteger = (value, min, max) =>
  /^[+-]?\d+$/.test(value.trim()) && Number(value) >= min && Number(value) <= max;
const trimName = (value) => value.replace(/^[ \t]+|[ \t]+$/g, '');
const isName = (value) => {
  const length = Buffer.byteLength(trimName(value));
  return length > 0 && length < 30;
};

// Validate positional input before sending it to any supported C executable.
function validateAnswers(action, answers) {
  const [first, second, third, fourth, fifth] = answers;
  switch (action) {
    case 'searchResource':
    case 'addResource':
    case 'bookResource':
      if (!isInteger(first, 1, 9999)) return 'Resource ID must be a whole number from 1 to 9999.';
      break;
    case 'cancelBooking':
      if (!isInteger(first, 1, 999999)) return 'Booking ID must be a whole number from 1 to 999999.';
      break;
  }
  if (action === 'addResource') {
    if (!isName(second)) return 'Enter a resource name up to 29 bytes.';
    if (!isInteger(third, 1, 2)) return 'Choose Classroom or Laboratory.';
    if (!isInteger(fourth, 1, 500)) return 'Capacity must be a whole number from 1 to 500.';
  }
  if (action === 'bookResource') {
    if (!validDate(second.trim())) return 'Choose a valid date from 2024 to 2100.';
    if (!isInteger(third, 1, 4)) return 'Choose a time slot.';
    if (!isName(fourth)) return 'Enter a booking name up to 29 bytes.';
    if (fifth && !isInteger(fifth, 0, 1)) return 'Choose whether to join the waiting list.';
  }
  if (action === 'availability' && !validDate(first.trim())) return 'Choose a valid date from 2024 to 2100.';
  return '';
}

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
    child.stdout.setEncoding('utf8');
    child.stdout.on('data', (d) => (output += d));
    child.stdin.on('error', () => {});
    child.on('error', () => {
      output = 'Cannot start the C program. Did you compile it? (see README steps)';
    });
    child.on('close', (code) => {
      if (code === 0 && Object.hasOwn(WRITE_ROLES, action) && !output.includes('Warning: cannot write ')) {
        if (!refreshStorage(__dirname)) output += '\nWarning: SQLite synchronization failed. See the server log.\n';
      }
      resolve({ output: output || 'The C program returned no output.', code });
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
    const reply = (status, output, details = {}) => {
      res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
      res.end(JSON.stringify({ output, ...details }));
    };
    let length = 0;
    const chunks = [];
    req.on('data', (chunk) => {
      length += chunk.length;
      if (length <= MAX_BODY_BYTES) chunks.push(chunk);
    });
    req.on('end', async () => {
      if (length > MAX_BODY_BYTES) return reply(413, 'Request is too large.');
      let data;
      try { data = JSON.parse(Buffer.concat(chunks)); } catch { return reply(400, 'Bad request.'); }
      if (!data || typeof data !== 'object' || Array.isArray(data)) return reply(400, 'Bad request.');
      const cookie = (req.headers.cookie || '').match(/(?:^|;\s*)booking_session=([\w-]+)/)?.[1];
      if (action === 'logout') {
        sessions.delete(cookie);
        res.setHeader('Set-Cookie', 'booking_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0');
        return reply(200, 'Logged out.');
      }
      if (!Object.hasOwn(ACTIONS, action)) return reply(400, 'Bad request.');
      const session = sessions.get(cookie);
      if (action !== 'login' && (!session || session.expires <= Date.now())) {
        sessions.delete(cookie);
        return reply(401, 'Please log in.');
      }
      if (action === 'login' && !['string', 'number'].includes(typeof data.role)) return reply(400, 'Bad request.');
      const role = action === 'login' ? String(data.role) : session.role;
      if (!/^[123]$/.test(role)) return reply(400, 'Bad request.');
      if (Object.hasOwn(WRITE_ROLES, action) && !WRITE_ROLES[action].includes(role)) return reply(403, 'Access denied!\n');
      const fields = ACTIONS[action].map((f) => data[f] ?? '');
      if (fields.some((value) => !['string', 'number'].includes(typeof value) || String(value).includes('\0'))) {
        return reply(400, 'Invalid input.');
      }
      const answers = fields.map((value) => String(value).replace(/[\r\n\v\f]/g, ' '));
      const inputError = validateAnswers(action, answers);
      if (inputError) return reply(400, 'Invalid input! ' + inputError);
      if (action === 'availability') {
        try {
          await queue;
          return reply(200, '', readAvailability(__dirname, answers[0].trim()));
        } catch (error) {
          console.error('Cannot read availability:', error.message);
          return reply(500, 'Availability is unavailable. Check the primary data files.');
        }
      }
      const input = answers.map((value, index) => {
        const field = ACTIONS[action][index];
        if (field === 'name') return trimName(value);
        if (field === 'password') return value;
        if (field === 'date') return value.trim();
        return String(Number(value));
      });
      const { output, code } = await runC(role, action, input);
      if (action === 'login') {
        if (!/^LOGIN_OK\s*$/m.test(output)) return reply(code === null ? 500 : 401, output);
        sessions.delete(cookie);
        for (const [id, entry] of sessions) {
          if (entry.expires <= Date.now()) sessions.delete(id);
        }
        const token = randomUUID();
        sessions.set(token, { role, expires: Date.now() + SESSION_SECONDS * 1000 });
        res.setHeader('Set-Cookie', `booking_session=${token}; HttpOnly; SameSite=Strict; Path=/; Max-Age=${SESSION_SECONDS}`);
        return reply(200, output, { role: Number(role) });
      }
      if (code === null || code < 0 || code === 2) return reply(500, output);
      if (code !== 0) return reply(400, output);
      if (output.includes('Warning: cannot write ')) return reply(500, output);
      return reply(output.includes('Access denied!') ? 403 : 200, output);
    });
    return;
  }

  // 2) Otherwise send a frontend file
  const page = Object.hasOwn(PAGES, req.url) ? PAGES[req.url] : null;
  if (page) {
    res.writeHead(200, { 'Content-Type': page[1] });
    res.end(fs.readFileSync(path.join(FRONTEND, page[0])));
  } else {
    res.writeHead(404);
    res.end('Not found');
  }
});

refreshStorage(__dirname);
server.listen(PORT, '127.0.0.1', () => console.log('Open http://127.0.0.1:' + PORT));
