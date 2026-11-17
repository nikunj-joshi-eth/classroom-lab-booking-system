CREATE TABLE IF NOT EXISTS resources (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  type TEXT NOT NULL CHECK (type IN ('Classroom', 'Laboratory')),
  capacity INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS bookings (
  id INTEGER PRIMARY KEY,
  resource_id INTEGER NOT NULL REFERENCES resources(id),
  date TEXT NOT NULL,
  slot INTEGER NOT NULL CHECK (slot BETWEEN 1 AND 4),
  booked_by TEXT NOT NULL,
  status TEXT NOT NULL CHECK (status IN ('Active', 'Cancelled'))
);

CREATE UNIQUE INDEX IF NOT EXISTS one_active_booking_per_slot
  ON bookings (resource_id, date, slot) WHERE status = 'Active';

CREATE TABLE IF NOT EXISTS waiting_requests (
  position INTEGER PRIMARY KEY,
  resource_id INTEGER NOT NULL REFERENCES resources(id),
  date TEXT NOT NULL,
  slot INTEGER NOT NULL CHECK (slot BETWEEN 1 AND 4),
  booked_by TEXT NOT NULL,
  UNIQUE (resource_id, date, slot, booked_by)
);
