/**
 * Collonka MCE Server
 *
 * A local HTTP server that bridges the Collonka VST plugin to Claude API.
 *
 * Endpoints:
 *   GET  /health  — returns { status: "ok" }
 *   POST /chat    — sends user message + param context to Claude, returns response
 *   POST /apply   — asks Claude to suggest param changes, returns JSON params
 *
 * Usage:
 *   1. Set ANTHROPIC_API_KEY environment variable
 *   2. npm install
 *   3. npm start
 *
 * The VST plugin connects to http://localhost:9150
 */

const http = require('http');
const Anthropic = require('@anthropic-ai/sdk');

const PORT = process.env.MCE_PORT || 9150;
const API_KEY = process.env.ANTHROPIC_API_KEY;

if (!API_KEY) {
    console.error('ERROR: ANTHROPIC_API_KEY environment variable is required.');
    console.error('Set it with: export ANTHROPIC_API_KEY=sk-ant-...');
    process.exit(1);
}

const anthropic = new Anthropic({ apiKey: API_KEY });

// System prompt that gives Claude context about the VST
const SYSTEM_PROMPT = `You are an AI assistant integrated into the Collonka synthesizer plugin — a Moog Minimoog-inspired monophonic bass synthesizer for R&B, Neo-Soul, Hip-Hop, and Trap production.

You help producers shape their bass sounds by:
- Explaining what parameters do in musical terms
- Suggesting parameter changes for specific sonic goals
- Recommending presets and starting points for different genres/styles
- Teaching synthesis concepts in the context of bass production

The synth has these parameter groups:
- Oscillators: 3 oscillators (OSC1/2/3) with waveforms (sine, triangle, saw, reverse saw, square, pulse), range (octave), level, detune, tuning
- Filter: Moog ladder filter with cutoff, resonance, envelope amount, keyboard tracking, slope (12/24 dB), saturation
- Envelopes: Filter ADSR (env_f_attack/decay/sustain/release) and Amp ADSR (env_a_attack/decay/sustain/release)
- Pitch Envelope: amount (semitones) and time for 808 punch
- Glide/Portamento: on/off, time, legato mode
- LFO: rate, depth, waveform (tri/sq/sine), target (filter/pitch/amp), sync
- Output: drive, bass shelf EQ, presence shelf EQ, stereo width, master volume
- Tape Saturation: drive, saturation, bump (head bump), mix
- Compressor: threshold, attack, release, optical ratio, parallel mix, output gain
- Reverb: crossover freq, decay, pre-delay, wet amount
- Bass Mode: 0=Pluck, 1=808, 2=Reese

When the user's current parameter state is provided as context, reference specific values and suggest concrete changes.

When asked to generate or suggest parameters, respond with a JSON block in this format:
\`\`\`params
{"param_id": value, "param_id": value, ...}
\`\`\`

Keep responses concise and musical. You're talking to producers, not engineers.`;

// Parse JSON body from request
function parseBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            try {
                resolve(body ? JSON.parse(body) : {});
            } catch (e) {
                reject(new Error('Invalid JSON'));
            }
        });
        req.on('error', reject);
    });
}

// Send JSON response
function sendJSON(res, statusCode, data) {
    res.writeHead(statusCode, {
        'Content-Type': 'application/json',
        'Access-Control-Allow-Origin': '*'
    });
    res.end(JSON.stringify(data));
}

// Call Claude API
async function askClaude(userMessage, paramContext) {
    let contextStr = '';
    if (paramContext && typeof paramContext === 'object') {
        contextStr = '\n\nCurrent synth parameters:\n```json\n' +
            JSON.stringify(paramContext, null, 2) + '\n```';
    }

    const response = await anthropic.messages.create({
        model: 'claude-sonnet-4-20250514',
        max_tokens: 1024,
        system: SYSTEM_PROMPT,
        messages: [
            {
                role: 'user',
                content: userMessage + contextStr
            }
        ]
    });

    return response.content[0].text;
}

// Extract param JSON from Claude response if present
function extractParams(responseText) {
    const match = responseText.match(/```params\s*\n?([\s\S]*?)\n?```/);
    if (match) {
        try {
            return JSON.parse(match[1]);
        } catch (e) {
            return null;
        }
    }
    return null;
}

// Extract notes JSON from Claude response
function extractNotes(responseText) {
    const match = responseText.match(/```notes\s*\n?([\s\S]*?)\n?```/);
    if (match) {
        try {
            return JSON.parse(match[1]);
        } catch (e) {
            return null;
        }
    }
    return null;
}

const PLAY_SYSTEM_PROMPT = SYSTEM_PROMPT + `

When asked to play or demonstrate a bass line, generate a sequence of MIDI note events.
Return them in a notes JSON block using this exact format:

\`\`\`notes
[
  {"note": 36, "vel": 100, "time": 0},
  {"note": 0, "vel": 0, "time": 400},
  {"note": 38, "vel": 90, "time": 500},
  {"note": 0, "vel": 0, "time": 900}
]
\`\`\`

Rules for note generation:
- "note" is MIDI note number (C1=36, C2=48, C3=60). Bass range: 24-60.
- "vel" is velocity 0-127. vel=0 means note-off.
- "time" is milliseconds from the start of the sequence.
- Every note-on MUST have a matching note-off (vel=0) at a later time.
- Keep sequences 2-8 bars (2000-16000ms at 120bpm).
- Match the style to the user's request and the current bass mode.
- For 808: use long sustained notes (400-800ms), slides via close note spacing.
- For Pluck: use shorter notes (100-300ms), staccato patterns, syncopation.
- For Reese: use legato (overlapping note-on before note-off of previous), sustained.
- Root notes for R&B: C, Eb, F, G, Bb are common. Use the minor pentatonic.

Always include a brief text description of what you're playing before the notes block.`;


// HTTP Server
const server = http.createServer(async (req, res) => {
    // CORS preflight
    if (req.method === 'OPTIONS') {
        res.writeHead(204, {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        });
        res.end();
        return;
    }

    try {
        // GET /health
        if (req.method === 'GET' && req.url === '/health') {
            sendJSON(res, 200, { status: 'ok', server: 'bw-bass-mce', version: '1.0.0' });
            return;
        }

        // POST /chat
        if (req.method === 'POST' && req.url === '/chat') {
            const body = await parseBody(req);
            const message = body.message || '';
            const context = body.context || null;

            if (!message) {
                sendJSON(res, 400, { error: 'No message provided' });
                return;
            }

            console.log(`[CHAT] "${message.substring(0, 60)}..."`);

            const responseText = await askClaude(message, context);
            const params = extractParams(responseText);

            sendJSON(res, 200, {
                sender: 'Claude',
                message: responseText,
                has_params: params !== null,
                params: params
            });
            return;
        }

        // POST /apply
        if (req.method === 'POST' && req.url === '/apply') {
            const body = await parseBody(req);
            const context = body.context || null;

            const prompt = 'Based on the current synth parameters, suggest improvements ' +
                'to make this bass sound more polished and production-ready for R&B. ' +
                'Return your suggestions as a params JSON block with the specific parameter IDs and values to change. ' +
                'Only include parameters that should change.';

            console.log('[APPLY] Requesting param suggestions...');

            const responseText = await askClaude(prompt, context);
            const params = extractParams(responseText);

            sendJSON(res, 200, {
                sender: 'Claude',
                message: responseText,
                params: params || {}
            });
            return;
        }

        // POST /play
        if (req.method === 'POST' && req.url === '/play') {
            const body = await parseBody(req);
            const description = body.description || 'Play a simple R&B bass line';
            const context = body.context || null;

            console.log(`[PLAY] "${description.substring(0, 60)}..."`);

            const response = await anthropic.messages.create({
                model: 'claude-sonnet-4-20250514',
                max_tokens: 2048,
                system: PLAY_SYSTEM_PROMPT,
                messages: [
                    {
                        role: 'user',
                        content: description +
                            (context ? '\n\nCurrent synth parameters:\n```json\n' +
                                JSON.stringify(context, null, 2) + '\n```' : '')
                    }
                ]
            });

            const responseText = response.content[0].text;
            const notes = extractNotes(responseText);

            // Strip the notes block from the message text for cleaner display
            const cleanMessage = responseText.replace(/```notes[\s\S]*?```/g, '').trim();

            sendJSON(res, 200, {
                sender: 'Claude',
                message: cleanMessage,
                notes: notes || []
            });
            return;
        }

        // 404
        sendJSON(res, 404, { error: 'Not found' });

    } catch (err) {
        console.error('[ERROR]', err.message);
        sendJSON(res, 500, { error: err.message });
    }
});

server.listen(PORT, '127.0.0.1', () => {
    console.log(`\n  Collonka MCE Server v1.0.0`);
    console.log(`  Listening on http://127.0.0.1:${PORT}`);
    console.log(`  Claude model: claude-sonnet-4-20250514`);
    console.log(`\n  Waiting for Collonka plugin to connect...\n`);
});
