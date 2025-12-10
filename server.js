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
const ISSUES_COLLECTION_NAME = 'issues';

const client = new MongoClient(MONGO_URI);
let booksCollection;
let issuesCollection;

// connect to Mongo once when server starts
async function connectMongo() {
  try {
    await client.connect();
    const db = client.db(DB_NAME);
    booksCollection = db.collection(COLLECTION_NAME);
    issuesCollection = db.collection(ISSUES_COLLECTION_NAME);
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

// POST /api/books/:id/issue -> issue book + store student info
// Body: { studentName, usn }
app.post('/api/books/:id/issue', async (req, res) => {
  try {
    const id = Number(req.params.id);
    const { studentName, usn } = req.body || {};

    if (!studentName || !usn) {
      return res.status(400).json({ error: 'studentName and usn are required' });
    }

    if (!booksCollection) {
      return res.status(500).json({ error: 'MongoDB not connected' });
    }

    // 1) Check if book exists in Mongo
    const bookDoc = await booksCollection.findOne({ id });
    if (!bookDoc) {
      return res.status(404).json({ error: `Book with ID ${id} not found` });
    }

    // 2) Check if any copies are available
    if (bookDoc.available <= 0) {
      return res.status(400).json({ error: `Book with ID ${id} is out of stock` });
    }

    // 3) Call C backend to actually issue (decrease availableCopies)
    const output = await runCli(['issue', String(id)]);

    // 4) Sync books collection with new state
    await syncMongoWithList(output);

    const data = output && output.trim() ? JSON.parse(output) : [];

    // 5) Store issue entry only if everything above succeeded
    if (issuesCollection) {
      await issuesCollection.insertOne({
        bookId: id,
        studentName,
        usn,
        issuedAt: new Date(),
      });
    }

    // 6) Return updated list of books to frontend
    res.json(data);
  } catch (err) {
    console.error('issue error:', err);
    res.status(500).json({ error: 'Failed to issue book' });
  }
});

// POST /api/books/:id/return -> return book for a specific student USN
// Body: { usn }
app.post('/api/books/:id/return', async (req, res) => {
  try {
    const id = Number(req.params.id);
    const { usn } = req.body || {};

    if (!usn) {
      return res.status(400).json({ error: 'usn is required to return a book' });
    }

    if (!issuesCollection) {
      return res.status(500).json({ error: 'Issues collection not ready' });
    }

    // 1) Find the latest (most recent) issue for this bookId + usn that is not yet returned
    const openIssue = await issuesCollection.findOne(
      { bookId: id, usn, returnedAt: { $exists: false } },
      { sort: { issuedAt: -1 } }
    );

    if (!openIssue) {
      return res.status(404).json({
        error: `No active issue found for book ID ${id} and USN ${usn}`,
      });
    }

    // 2) Call C backend to increase availableCopies
    const output = await runCli(['return', String(id)]);

    // 3) Sync books collection with new state
    await syncMongoWithList(output);
    const data = output && output.trim() ? JSON.parse(output) : [];

    // 4) Mark this issue record as returned (or you could delete it instead)
    await issuesCollection.updateOne(
      { _id: openIssue._id },
      { $set: { returnedAt: new Date() } }
    );

    // 5) Send updated books list back
    res.json(data);
  } catch (err) {
    console.error('return error:', err);
    res.status(500).json({ error: 'Failed to return book' });
  }
});

// GET /api/issues -> list all borrower/issue records with book info
app.get('/api/issues', async (req, res) => {
  try {
    if (!issuesCollection) {
      return res.status(500).json({ error: 'Issues collection not available' });
    }

    // 1) Get all issue records (latest first)
    const issues = await issuesCollection
      .find({})
      .sort({ issuedAt: -1 })
      .toArray();

    if (issues.length === 0) {
      return res.json([]); // no borrowers yet
    }

    // 2) Collect unique bookIds from issues
    const bookIds = [...new Set(issues.map(i => i.bookId))];

    // 3) Fetch those books from books collection
    const books = await booksCollection
      .find({ id: { $in: bookIds } })
      .toArray();

    const bookMap = new Map();
    books.forEach(b => {
      bookMap.set(b.id, b);
    });

    // 4) Build response list with book title + borrower info
    const result = issues.map(issue => {
      const book = bookMap.get(issue.bookId);
      return {
        bookId: issue.bookId,
        bookTitle: book ? book.title : 'Unknown',
        studentName: issue.studentName,
        usn: issue.usn,
        issuedAt: issue.issuedAt,
      };
    });

    res.json(result);
  } catch (err) {
    console.error('issues list error:', err);
    res.status(500).json({ error: 'Failed to list issues' });
  }
});

app.listen(PORT, () => {
  console.log(`Server running on http://localhost:${PORT}`);
  console.log(`Using C backend at: ${BINARY_PATH}`);
  connectMongo();
});
