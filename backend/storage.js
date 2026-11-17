const fs = require('node:fs');
const path = require('node:path');

const slots = ['9-10_AM', '10-11_AM', '11-12_PM', '2-3_PM'];
const validInteger = (value, max) => /^\+?[0-9]+$/.test(value) && Number(value) >= 1 && Number(value) <= max;
const validName = (name) => Buffer.byteLength(name) < 30 && !name.includes('\0');

function validDate(date) {
  if (!/^\d{2}-\d{2}-\d{4}$/.test(date)) return false;
  const [day, month, year] = date.split('-').map(Number);
  return year >= 2024 && year <= 2100 && month >= 1 && month <= 12 &&
    day >= 1 && day <= new Date(Date.UTC(year, month, 0)).getUTCDate();
}

function readRows(directory, file, header) {
  let contents;
  try {
    contents = fs.readFileSync(path.join(directory, file), 'utf8');
  } catch (error) {
    if (error.code === 'ENOENT') return [];
    throw error;
  }
  return contents.split(/\r?\n/)
    .map((line) => line.replace(/^[ \t\r\v\f]+|[ \t\r\v\f]+$/g, '').split(/[ \t\r\v\f]+/))
    .filter(([first]) => first && first !== header);
}

function importRows(db, directory) {
  const resourceIds = new Set();
  let skipped = 0;
  const insertResource = db.prepare('INSERT INTO resources VALUES (?, ?, ?, ?) ON CONFLICT DO NOTHING');
  for (const row of readRows(directory, 'resources.txt', 'ID')) {
    const [id, name, type, capacity] = row;
    if (row.length === 4 && resourceIds.size < 50 && validInteger(id, 9999) && validName(name) &&
        (type === 'Classroom' || type === 'Laboratory') && validInteger(capacity, 500) &&
        insertResource.run(Number(id), name, type, Number(capacity)).changes) {
      resourceIds.add(Number(id));
    } else {
      skipped++;
    }
  }

  const insertBooking = db.prepare('INSERT INTO bookings VALUES (?, ?, ?, ?, ?, ?) ON CONFLICT DO NOTHING');
  let bookingCount = 0;
  for (const row of readRows(directory, 'bookings.txt', 'BookingID')) {
    const [id, resourceId, , date, slotText, name, status] = row;
    const slot = slots.indexOf(slotText) + 1;
    if (row.length === 7 && bookingCount < 200 && validInteger(id, 99999) &&
        validInteger(resourceId, 9999) && resourceIds.has(Number(resourceId)) &&
        validDate(date) && slot && validName(name) && (status === 'Active' || status === 'Cancelled') &&
        insertBooking.run(Number(id), Number(resourceId), date, slot, name, status).changes) {
      bookingCount++;
    } else {
      skipped++;
    }
  }

  const insertWaiting = db.prepare('INSERT INTO waiting_requests VALUES (?, ?, ?, ?, ?) ON CONFLICT DO NOTHING');
  let position = 1;
  for (const row of readRows(directory, 'waiting.txt', 'No.')) {
    const [, resourceId, , date, slotText, name] = row;
    const slot = slots.indexOf(slotText) + 1;
    if (row.length === 6 && /^\d+$/.test(row[0]) && validInteger(resourceId, 9999) &&
        resourceIds.has(Number(resourceId)) && validDate(date) && slot && validName(name) &&
        insertWaiting.run(position, Number(resourceId), date, slot, name).changes) {
      position++;
    } else {
      skipped++;
    }
  }
  return skipped;
}

function refreshStorage(directory) {
  try {
    const { DatabaseSync } = require('node:sqlite');
    const db = new DatabaseSync(path.join(directory, 'booking.db'));
    try {
      db.exec('BEGIN IMMEDIATE');
      db.exec(fs.readFileSync(path.join(__dirname, 'schema.sql'), 'utf8'));
      db.exec('DELETE FROM waiting_requests; DELETE FROM bookings; DELETE FROM resources;');
      const skipped = importRows(db, directory);
      db.exec('COMMIT');
      if (skipped) console.warn(`SQLite synchronization skipped ${skipped} invalid or duplicate record(s).`);
    } finally {
      db.close(); // SQLite rolls back any transaction that did not commit.
    }
  } catch (error) {
    console.error(`SQLite synchronization failed: ${error.message}`);
  }
}

module.exports = { refreshStorage };
