const express = require('express');
const cors = require('cors');
const { execFile } = require('child_process');
const path = require('path');

const app = express();
const PORT = 3000;

// detect binary name (Windows vs others)
const BINARY_NAME = process.platform === 'win32' ? 'library_cli.exe' : './library_cli';
const BINARY_PATH = path.join(__dirname, BINARY_NAME);

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

// ---------- Routes ----------

// GET /api/books -> list all books as JSON
app.get('/api/books', async (req, res) => {
  try {
    const output = await runCli(['list']);
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
});
