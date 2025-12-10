const express = require('express');
const cors = require('cors');
const { execFile } = require('child_process');
const path = require('path');
const { MongoClient } = require('mongodb');

const app = express();
const PORT = 3000;

// detect binary name (Windows vs others)
const BINARY_NAME = process.platform === 'win32' ? 'library_cli.exe' : './library_cli';
const BINARY_PATH = path.join(__dirname, BINARY_NAME);

const MONGO_URI = 'mongodb://localhost:27017'; // adjust if needed
const DB_NAME = 'librarydb';
const COLLECTION_NAME = 'books';

const client = new MongoClient(MONGO_URI);
let booksCollection;

// connect to Mongo once when server starts
async function connectMongo() {
  try {
    await client.connect();
    const db = client.db(DB_NAME);
    booksCollection = db.collection(COLLECTION_NAME);
    console.log('Connected to MongoDB');
  } catch (err) {
    console.error('MongoDB connection error:', err);
  }
}

app.use(cors());
app.use(express.json()); // to parse JSON bodies

// helper: run the C CLI and return a Promise with stdout
function runCli(args) {
  return new Promise((resolve, reject) => {
    execFile(BINARY_PATH, args, { cwd: __dirname }, (error, stdout, stderr) => {
      if (error) {
        console.error('Error running C backend:', error);
        console.error('Stderr:', stderr);
        return reject(error);
      }
      resolve(stdout.toString());
    });
  });
}

async function syncMongoWithList(jsonString) {
  if (!booksCollection) {
    console.warn('Mongo not connected, skipping Mongo sync');
    return;
  }

  const trimmed = (jsonString || '').trim();
  if (!trimmed) {
    // empty list -> clear collection
    await booksCollection.deleteMany({});
    return;
  }

  let books;
  try {
    books = JSON.parse(trimmed);
  } catch (err) {
    console.error('Failed to parse JSON from C:', err);
    return;
  }

  if (!Array.isArray(books)) return;

  // Clear and insert new snapshot
  await booksCollection.deleteMany({});
  if (books.length > 0) {
    await booksCollection.insertMany(books);
  }
}


// ---------- Routes ----------

// GET /api/books -> list all books as JSON
app.get('/api/books', async (req, res) => {
  try {
    const output = await runCli(['list']);
    await syncMongoWithList(output);
    const data = output && output.trim() ? JSON.parse(output) : [];
    res.json(data);
  } catch (err) {
    console.error('list error:', err);
    res.status(500).json({ error: 'Failed to list books' });
  }
});

// POST /api/books -> add a new book
// Body: { id, title, author, category, totalCopies }
app.post('/api/books', async (req, res) => {
  try {
    const { id, title, author, category, totalCopies } = req.body;
    if (!id || !title || !author || !category || !totalCopies) {
      return res.status(400).json({ error: 'Missing fields' });
    }

    // library_cli expects args without spaces -> use underscores for now
    const safeTitle = String(title).replace(/\s+/g, '_');
    const safeAuthor = String(author).replace(/\s+/g, '_');
    const safeCategory = String(category).replace(/\s+/g, '_');

    const output = await runCli([
      'add',
      String(id),
      safeTitle,
      safeAuthor,
      safeCategory,
      String(totalCopies),
    ]);

    await syncMongoWithList(output);

    const data = output && output.trim() ? JSON.parse(output) : [];
    res.json(data);
  } catch (err) {
    console.error('add error:', err);
    res.status(500).json({ error: 'Failed to add book' });
  }
});

// DELETE /api/books/:id -> delete book
app.delete('/api/books/:id', async (req, res) => {
  try {
    const id = req.params.id;
    const output = await runCli(['delete', String(id)]);
    await syncMongoWithList(output);
    const data = output && output.trim() ? JSON.parse(output) : [];
    res.json(data);
  } catch (err) {
    console.error('delete error:', err);
    res.status(500).json({ error: 'Failed to delete book' });
  }
});

// POST /api/books/:id/issue -> issue book
app.post('/api/books/:id/issue', async (req, res) => {
  try {
    const id = req.params.id;
    const output = await runCli(['issue', String(id)]);
    await syncMongoWithList(output);
    const data = output && output.trim() ? JSON.parse(output) : [];
    res.json(data);
  } catch (err) {
    console.error('issue error:', err);
    res.status(500).json({ error: 'Failed to issue book' });
  }
});

// POST /api/books/:id/return -> return book
app.post('/api/books/:id/return', async (req, res) => {
  try {
    const id = req.params.id;
    const output = await runCli(['return', String(id)]);
    await syncMongoWithList(output);
    const data = output && output.trim() ? JSON.parse(output) : [];
    res.json(data);
  } catch (err) {
    console.error('return error:', err);
    res.status(500).json({ error: 'Failed to return book' });
  }
});

app.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
  console.log(`Using C backend at: ${BINARY_PATH}`);
  connectMongo();
});
