// All menu options. "roles" = who can use it (1 Admin, 2 Teacher, 3 Student).
// "fields" = the inputs shown in the form. Field names must match the names in server.js.
const SLOTS = ['1|9-10 AM', '2|10-11 AM', '3|11-12 PM', '4|2-3 PM'];

const ACTIONS = [
  { key: 'viewResources',  label: 'View Resources',  roles: [1, 2, 3] },
  { key: 'searchResource', label: 'Search Resource', roles: [1, 2, 3],
    fields: [{ name: 'id', label: 'Resource ID', type: 'number' }] },
  { key: 'addResource',    label: 'Add Classroom/Lab', roles: [1],
    fields: [
      { name: 'id',       label: 'Resource ID (1-9999)', type: 'number' },
      { name: 'name',     label: 'Name (e.g. Room101)',  type: 'text' },
      { name: 'type',     label: 'Type', options: ['1|Classroom', '2|Laboratory'] },
      { name: 'capacity', label: 'Capacity (1-500)',     type: 'number' }
    ] },
  { key: 'sortResources',  label: 'Sort Resources',  roles: [1] },
  { key: 'bookResource',   label: 'Book Resource',   roles: [1, 2],
    fields: [
      { name: 'resId', label: 'Resource ID', type: 'number' },
      { name: 'date',  label: 'Date',        type: 'date' },
      { name: 'slot',  label: 'Time slot',   options: SLOTS },
      { name: 'name',  label: 'Your name',   type: 'text' },
      { name: 'joinWaiting', label: 'Join the waiting list if the room is taken', type: 'checkbox' }
    ] },
  { key: 'viewBookings',   label: 'View Bookings',   roles: [1, 2, 3] },
  { key: 'cancelBooking',  label: 'Cancel Booking',  roles: [1, 2],
    fields: [{ name: 'id', label: 'Booking ID', type: 'number' }] },
  { key: 'viewWaitingList',   label: 'View Waiting List',   roles: [1, 2, 3] },
  { key: 'bookingHistory',    label: 'Booking History',     roles: [1, 2, 3] },
  { key: 'bookingStatistics', label: 'Booking Statistics',  roles: [1, 2, 3] }
];

const ROLE_NAMES = { 1: 'Administrator', 2: 'Teacher', 3: 'Student' };
let role = 0;                                   // 0 = not logged in

const $ = (id) => document.getElementById(id);  // short helper

// Send data to the backend and return the text printed by the C program
async function callBackend(action, data) {
  data.role = role;
  const response = await fetch('/api/' + action, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  });
  const result = await response.json();
  return result.output;
}

// Run an action and show the result
async function run(action, data) {
  $('output').textContent = 'Please wait...';
  $('output').textContent = await callBackend(action, data);
}

// ----- Login / Logout -----
$('role').onchange = () => {                    // students need no password
  $('passRow').classList.toggle('hidden', $('role').value === '3');
};

$('loginBtn').onclick = async () => {
  role = Number($('role').value);
  const text = await callBackend('login', { password: $('password').value });
  if (text.includes('LOGIN_OK')) {
    $('loginBox').classList.add('hidden');
    $('app').classList.remove('hidden');
    $('who').textContent = 'Logged in as: ' + ROLE_NAMES[role];
    $('loginMsg').textContent = '';
    $('password').value = '';
    showMenu();
  } else {
    role = 0;
    $('loginMsg').textContent = 'Incorrect password!';
  }
};

$('logoutBtn').onclick = () => {
  role = 0;
  $('app').classList.add('hidden');
  $('loginBox').classList.remove('hidden');
  $('formBox').classList.add('hidden');
  $('output').textContent = 'Choose an option above.';
};

// ----- Menu and forms -----
function showMenu() {
  $('menu').innerHTML = '';
  ACTIONS.filter((a) => a.roles.includes(role)).forEach((a) => {
    const btn = document.createElement('button');
    btn.textContent = a.label;
    btn.onclick = () => openAction(a);
    $('menu').appendChild(btn);
  });
}

function openAction(a) {
  const box = $('formBox');
  if (!a.fields) {                              // no input needed: run immediately
    box.classList.add('hidden');
    run(a.key, {});
    return;
  }
  box.innerHTML = '<h3>' + a.label + '</h3>';
  a.fields.forEach((f) => {
    const label = document.createElement('label');
    label.textContent = f.label;
    let input;
    if (f.options) {                            // dropdown
      input = document.createElement('select');
      f.options.forEach((o) => {
        const [value, text] = o.split('|');
        input.add(new Option(text, value));
      });
    } else {
      input = document.createElement('input');
      input.type = f.type;
      if (f.type === 'date') { input.min = '2024-01-01'; input.max = '2100-12-31'; }
    }
    input.id = 'f_' + f.name;
    box.appendChild(label);
    box.appendChild(input);
  });
  const submit = document.createElement('button');
  submit.textContent = 'Submit';
  submit.onclick = () => {
    const data = {};
    const empty = a.fields.some((f) => f.type !== 'checkbox' && !$('f_' + f.name).value.trim());
    if (empty) { $('output').textContent = 'Please fill in all fields.'; return; }
    a.fields.forEach((f) => {
      const input = $('f_' + f.name);
      if (f.type === 'checkbox') data[f.name] = input.checked ? 1 : 0;
      else if (f.type === 'date') data[f.name] = input.value.split('-').reverse().join('-'); // YYYY-MM-DD -> DD-MM-YYYY
      else data[f.name] = input.value;
    });
    run(a.key, data);
  };
  box.appendChild(submit);
  box.classList.remove('hidden');
}
