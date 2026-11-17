const $ = (id) => document.getElementById(id);
const ROLE_NAMES = { 1: 'Administrator', 2: 'Teacher', 3: 'Student' };
const actions = [
  { key: 'viewResources', label: 'View resources' },
  { key: 'searchResource', label: 'Search by ID', fields: [{ name: 'id', label: 'Resource ID', max: 9999 }] },
  { key: 'addResource', label: 'Add classroom or lab', roles: [1], submit: 'Add resource', fields: [
    { name: 'id', label: 'Resource ID', max: 9999 },
    { name: 'name', label: 'Room name', type: 'text' },
    { name: 'type', label: 'Room type', options: [[1, 'Classroom'], [2, 'Laboratory']] },
    { name: 'capacity', label: 'Capacity', max: 500 }
  ] },
  { key: 'sortResources', label: 'Sort resources by ID', roles: [1] },
  { key: 'bookResource', label: 'Book a room', roles: [1, 2], submit: 'Request booking', fields: [
    { name: 'resId', label: 'Classroom or lab' },
    { name: 'date', label: 'Booking date', type: 'date' },
    { name: 'slot', label: 'Time slot' },
    { name: 'name', label: 'Your name', type: 'text' },
    { name: 'joinWaiting', label: 'Join the waiting list if this slot is already booked', type: 'checkbox' }
  ] },
  { key: 'viewBookings', label: 'Active bookings' },
  { key: 'cancelBooking', label: 'Cancel a booking', roles: [1, 2], submit: 'Cancel booking', fields: [
    { name: 'id', label: 'Booking ID from Active bookings', max: 999999 }
  ] },
  { key: 'viewWaitingList', label: 'Waiting list' },
  { key: 'bookingHistory', label: 'Booking history' },
  { key: 'bookingStatistics', label: 'Booking statistics' }
];
const bookingAction = actions.find((action) => action.key === 'bookResource');
const writes = new Set(['addResource', 'sortResources', 'bookResource', 'cancelBooking']);
const dateBounds = { min: '2024-01-01', max: '2100-12-31' };
let role = 0;
let availability = null;
let availabilityRequest = 0;
let actionRequest = 0;

function localDate() {
  const now = new Date();
  return [now.getFullYear(), now.getMonth() + 1, now.getDate()]
    .map((value) => String(value).padStart(2, '0')).join('-');
}

const convertDate = (date) => date.split('-').reverse().join('-');
const displayName = (name) => name.replaceAll('_', ' ');
const mayUse = (action) => !action.roles || action.roles.includes(role);

async function api(action, data = {}) {
  try {
    const response = await fetch('/api/' + action, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data)
    });
    return { ...await response.json(), ok: response.ok, status: response.status };
  } catch {
    return { ok: false, output: 'Cannot contact the booking server. Try again.' };
  }
}

function notify(message = '', kind = 'success') {
  $('notification').textContent = message;
  $('notification').dataset.kind = kind;
}

function showLogin(message = '') {
  role = 0;
  availability = null;
  availabilityRequest++;
  actionRequest++;
  $('app').hidden = true;
  $('loginBox').hidden = false;
  $('loginMsg').textContent = message;
  $('password').value = '';
}

function handleSession(result) {
  if (result.status !== 401) return false;
  showLogin('Your session expired. Sign in again.');
  return true;
}

$('role').onchange = () => {
  $('passRow').hidden = $('role').value === '3';
  $('loginMsg').textContent = '';
};

$('loginForm').onsubmit = async (event) => {
  event.preventDefault();
  $('loginBtn').disabled = true;
  $('loginMsg').textContent = '';
  const result = await api('login', { role: Number($('role').value), password: $('password').value });
  $('loginBtn').disabled = false;
  if (!result.ok || !ROLE_NAMES[result.role]) {
    const invalid = result.output.includes('LOGIN_FAIL') || result.status === 401;
    $('loginMsg').textContent = invalid ? 'Incorrect password.' : result.output.trim();
    return;
  }
  role = result.role;
  $('password').value = '';
  $('loginBox').hidden = true;
  $('app').hidden = false;
  $('who').textContent = ROLE_NAMES[role];
  $('day').value = localDate();
  for (const id of ['query', 'typeFilter', 'capacityFilter', 'availabilityFilter']) $(id).value = '';
  notify();
  showMenu();
  showOverview();
};

$('logoutBtn').onclick = async () => {
  $('logoutBtn').disabled = true;
  const result = await api('logout');
  $('logoutBtn').disabled = false;
  if (result.ok) showLogin();
  else notify(result.output, 'error');
};

function showMenu() {
  $('menu').replaceChildren();
  const entries = [{ key: 'overview', label: 'Room availability' }, ...actions.filter(mayUse)];
  for (const action of entries) {
    const button = document.createElement('button');
    button.type = 'button';
    button.dataset.action = action.key;
    button.textContent = action.label;
    button.onclick = () => action.key === 'overview' ? showOverview() : openAction(action);
    $('menu').append(button);
  }
}

function selectMenu(key) {
  for (const button of $('menu').children) {
    if (button.dataset.action === key) button.setAttribute('aria-current', 'page');
    else button.removeAttribute('aria-current');
  }
}

function showOverview() {
  actionRequest++;
  $('overview').hidden = false;
  $('actionPanel').hidden = true;
  selectMenu('overview');
  return loadAvailability();
}

$('closeAction').onclick = showOverview;
$('day').onchange = loadAvailability;
$('refresh').onclick = loadAvailability;
for (const id of ['query', 'typeFilter', 'capacityFilter', 'availabilityFilter']) {
  $(id).addEventListener('input', () => renderTimetable());
}

async function loadAvailability() {
  const request = ++availabilityRequest;
  const field = $('day');
  $('tableDate').textContent = field.value ? new Date(field.value + 'T12:00:00')
    .toLocaleDateString(undefined, { dateStyle: 'full' }) : '';
  if (availability?.date !== convertDate(field.value)) availability = null;
  if (!field.checkValidity()) {
    renderTimetable('Choose a valid date from 2024 to 2100.');
    return;
  }
  renderTimetable('Loading timetable…');
  const result = await api('availability', { date: convertDate(field.value) });
  if (request !== availabilityRequest || handleSession(result)) return;
  if (!result.ok) {
    availability = null;
    renderTimetable(result.output);
    return;
  }
  availability = result;
  renderTimetable();
}

function renderTimetable(message = '') {
  const table = $('roomTable');
  const rows = $('roomRows');
  const head = $('roomHead');
  rows.replaceChildren();
  head.replaceChildren();
  table.hidden = true;
  $('tableMessage').textContent = message || (availability ? '' : 'Timetable unavailable.');
  for (const id of ['roomCount', 'availableCount', 'bookedCount']) $(id).textContent = '—';
  if (!availability) return;

  const { resources, slots } = availability;
  const booked = resources.reduce((total, room) => total + room.bookedSlots.length, 0);
  $('roomCount').textContent = resources.length;
  $('bookedCount').textContent = booked;
  $('availableCount').textContent = resources.length * slots.length - booked;

  const query = displayName($('query').value).trim().toLowerCase();
  const type = $('typeFilter').value;
  const capacity = $('capacityFilter');
  const minCapacity = Number(capacity.value);
  const state = $('availabilityFilter').value;
  if (!capacity.checkValidity()) {
    $('tableMessage').textContent = 'Minimum seats must be a whole number from 1 to 500.';
    return;
  }
  const matches = resources.filter((room) =>
    (displayName(room.name).toLowerCase().includes(query) || String(room.id).includes(query)) &&
    (!type || room.type === type) && room.capacity >= minCapacity &&
    (state !== 'open' || room.bookedSlots.length < slots.length) &&
    (state !== 'full' || room.bookedSlots.length === slots.length));
  $('tableMessage').textContent = message || (matches.length
    ? `Showing ${matches.length} of ${resources.length} rooms and labs`
    : resources.length ? 'No rooms match your filters.' : 'There are no classrooms or labs yet.');
  if (!matches.length) return;

  const header = head.insertRow();
  for (const label of ['Room / lab', ...slots.map((slot) => slot.label)]) {
    const cell = document.createElement('th');
    cell.scope = 'col';
    cell.textContent = label;
    header.append(cell);
  }
  for (const room of matches) {
    const row = rows.insertRow();
    const cell = document.createElement('th');
    const name = displayName(room.name);
    cell.scope = 'row';
    cell.textContent = name;
    const detail = document.createElement('span');
    detail.textContent = `#${room.id} · ${room.type} · ${room.capacity} seats`;
    cell.append(detail);
    row.append(cell);
    for (const slot of slots) {
      const taken = room.bookedSlots.includes(slot.id);
      const interactive = mayUse(bookingAction);
      const status = document.createElement(interactive ? 'button' : 'span');
      status.className = 'slot ' + (taken ? 'booked' : 'available');
      status.textContent = taken ? 'Booked' : 'Available';
      if (interactive) {
        status.type = 'button';
        status.setAttribute('aria-label', `${name}, ${slot.label}: ${taken ? 'booked, join waiting list' : 'available, book room'}`);
        status.onclick = () => openAction(bookingAction, {
          resId: room.id, date: convertDate(availability.date), slot: slot.id, joinWaiting: taken
        });
      }
      row.insertCell().append(status);
    }
  }
  table.hidden = false;
}

async function openAction(action, values = {}) {
  const request = ++actionRequest;
  const form = $('formBox');
  $('overview').hidden = true;
  $('actionPanel').hidden = false;
  $('actionTitle').textContent = action.label;
  $('output').hidden = true;
  form.hidden = true;
  form.replaceChildren();
  notify();
  selectMenu(action.key);

  if (!action.fields) return runAction(action, {}, request);
  if (action === bookingAction && !availability) {
    const result = await api('availability', { date: convertDate(values.date || $('day').value || localDate()) });
    if (request !== actionRequest || handleSession(result)) return;
    if (!result.ok) return notify(result.output, 'error');
    availability = result;
  }

  for (const field of action.fields) {
    const options = field.options || (field.name === 'resId'
      ? availability.resources.map((room) => [room.id, `${displayName(room.name)} (#${room.id})`])
      : field.name === 'slot' ? availability.slots.map((slot) => [slot.id, slot.label]) : null);
    const wrapper = document.createElement('div');
    const label = document.createElement('label');
    const input = document.createElement(options ? 'select' : 'input');
    input.id = 'f_' + field.name;
    input.name = field.name;
    label.htmlFor = input.id;
    label.textContent = field.label;
    if (options) {
      input.add(new Option(field.label === 'Time slot' ? 'Choose a slot' : 'Choose an option', ''));
      for (const [value, text] of options) input.add(new Option(text, value));
    } else {
      input.type = field.type || 'number';
      if (field.max) Object.assign(input, { min: 1, max: field.max, step: 1 });
      if (field.type === 'date') Object.assign(input, dateBounds);
      if (field.type === 'text') Object.assign(input, { maxLength: 29, pattern: '.*\\S.*' });
    }
    if (field.type === 'checkbox') {
      input.checked = Boolean(values[field.name]);
      wrapper.className = 'check-field';
      wrapper.append(input, label);
    } else {
      input.required = true;
      input.value = values[field.name] ?? (field.type === 'date' ? $('day').value || localDate() : '');
      wrapper.append(label, input);
    }
    form.append(wrapper);
  }

  const footer = document.createElement('div');
  footer.className = 'form-actions';
  const submit = document.createElement('button');
  submit.type = 'submit';
  submit.className = 'primary';
  submit.textContent = action.submit || 'Search';
  footer.append(submit);
  form.append(footer);
  form.hidden = false;
  if (action === bookingAction && !availability.resources.length) {
    submit.disabled = true;
    notify('A classroom or lab must be added before you can book.', 'warning');
  }
  form.onsubmit = async (event) => {
    event.preventDefault();
    if (submit.disabled) return;
    const data = Object.fromEntries(new FormData(form));
    if (action === bookingAction) {
      data.date = convertDate(data.date);
      data.joinWaiting = Number($('f_joinWaiting').checked);
    }
    submit.disabled = true;
    await runAction(action, data, request);
    submit.disabled = false;
  };
}

async function runAction(action, data, request) {
  notify('Working…');
  const result = await api(action.key, data);
  if (request !== actionRequest || handleSession(result)) return;
  const output = result.output.trim();
  if (writes.has(action.key)) {
    const completed = /added successfully|sorted by ID|booking confirmed|added to waiting list|booking \d+ cancelled\./i;
    const kind = !result.ok ? 'error' : completed.test(output) && !/^Warning:/m.test(output) ? 'success' : 'warning';
    notify(output, kind);
    availability = null;
  } else if (result.ok) {
    notify();
    $('output').textContent = output;
    $('output').hidden = false;
  } else {
    notify(output, 'error');
  }
}
