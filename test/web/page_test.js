// Browser tests for web/index.html, run against tools/mock_server.py.
//
//   node test/web/page_test.js            (needs Playwright + Chromium)
//
// Runs at two phone sizes (a small iPhone, a narrow Android) and checks the
// things guests will do: power, brightness dragging, modes, colours, presets,
// settings confirmations, share code, staying in sync with other phones,
// firmware updates (wrong files caught, a good one installed, the restart
// followed), and the offline banner. Saves screenshots (light and dark) to test/build/web/.
"use strict";
const { chromium, devices } = require("playwright");
const { spawn } = require("child_process");
const assert = require("assert");
const fs = require("fs");
const path = require("path");

const ROOT = path.resolve(__dirname, "../..");
const OUT = path.join(ROOT, "test/build/web");
const PORT = 8765;
const BASE = `http://127.0.0.1:${PORT}`;
const PHONES = ["iPhone 13 Mini", "Galaxy S8"];
// The firmware's marker text (a file without it is refused as another sketch's firmware).
const MARKER = Buffer.from(/kText\[\] = "([^"]+)"/.exec(
  fs.readFileSync(path.join(ROOT, "firmware/SpectrumAnalyzer/src/update/FirmwareMarker.h"), "utf8"))[1]);

// A pretend firmware file: ESP32 image magic byte, filler, and optionally the marker.
function fakeFirmware(size, withMarker) {
  const b = Buffer.alloc(size, 7);
  b[0] = 0xE9;
  if (withMarker) MARKER.copy(b, 5000);
  return b;
}

let failures = 0;
async function check(name, fn) {
  try {
    await fn();
    console.log(`  ok    ${name}`);
  } catch (e) {
    failures++;
    console.log(`  FAIL  ${name}\n        ${e.message.split("\n")[0]}`);
  }
}

function startServer() {
  const proc = spawn("python3", [path.join(ROOT, "tools/mock_server.py"), "--port", String(PORT)], {
    stdio: ["ignore", "pipe", "inherit"],
  });
  return new Promise((resolve) => proc.stdout.once("data", () => resolve(proc)));
}

async function requests(since = 0) {
  const res = await fetch(`${BASE}/__requests`);
  return (await res.json()).filter((r) => r.t >= since);
}

async function openPage(browser, phone, scheme = "light") {
  const context = await browser.newContext({ ...devices[phone], colorScheme: scheme });
  const page = await context.newPage();
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  // Failed requests (a refused upload, the gap while the lights restart) are expected;
  // Chrome logs each as "Failed to load resource". Anything else counts as an error.
  page.on("console", (m) => {
    if (m.type() === "error" && !m.text().startsWith("Failed to load resource")) errors.push(m.text());
  });
  await page.goto(BASE + "/");
  await page.waitForSelector("#add-preset");
  return { context, page, errors };
}

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  let server = await startServer();
  const browser = await chromium.launch();

  for (const phone of PHONES) {
    console.log(`\n${phone}`);
    // Fresh state for each phone.
    await fetch(`${BASE}/api/action/reset-settings`, { method: "POST" });
    const { context, page, errors } = await openPage(browser, phone);

    await check("first view: title, 6 modes, no presets yet but an add button, power on", async () => {
      assert.strictEqual(await page.textContent("h1"), "Spectrum Lights");
      assert.strictEqual(await page.locator("#modes .mode").count(), 6);
      assert.strictEqual(await page.locator("#presets .preset").count(), 0);
      assert.ok(await page.isVisible("#add-preset"));
      assert.strictEqual(await page.getAttribute("#power", "aria-checked"), "true");
    });

    await check("nothing wider than the screen", async () => {
      const [scroll, client] = await page.evaluate(() => [document.documentElement.scrollWidth,
                                                          document.documentElement.clientWidth]);
      assert.ok(scroll <= client, `page is ${scroll}px wide on a ${client}px screen`);
    });

    await check("tap targets are at least 40px tall", async () => {
      const small = await page.evaluate(() => [...document.querySelectorAll("button:not([hidden]), input")]
        .filter((el) => el.offsetParent !== null)
        .map((el) => [el.id || el.className || el.tagName, el.getBoundingClientRect().height])
        .filter(([, h]) => h < 40));
      assert.deepStrictEqual(small, []);
    });

    await check("power switch turns the lights off and on", async () => {
      await page.click("#power");
      await page.waitForFunction(() => document.querySelector("#power").getAttribute("aria-checked") === "false");
      await page.click("#power");
      await page.waitForFunction(() => document.querySelector("#power").getAttribute("aria-checked") === "true");
    });

    await check("modes: tapping one selects it and shows the right colour controls", async () => {
      await page.click('[data-mode="gradient"]');
      await page.waitForFunction(() => document.querySelector('[data-mode="gradient"]').getAttribute("aria-checked") === "true");
      assert.ok(await page.isVisible("#colour-a-row") && await page.isVisible("#colour-b-row"));
      assert.strictEqual(await page.textContent("#colour-a-label"), "Left end");
      await page.click('[data-mode="rainbow"]');
      await page.waitForFunction(() => document.querySelector("#colour-a-row").hidden);
      assert.ok(await page.isVisible("#colour-note"));
      await page.click('[data-mode="spectrum-snappy"]');
      await page.waitForSelector("#spectrum-colour", { state: "visible" });
      assert.ok(!(await page.isVisible("#colour-a-row")), "rainbow spectrum needs no colour picker");
      await page.click('[data-sc="fixed"]');
      await page.waitForSelector("#colour-a-row", { state: "visible" });
    });

    await check("dragging brightness sends at most ~10 requests a second, ending on the last value", async () => {
      const t0 = Date.now() / 1000;
      // 60 slider movements over 1.5 s, like a finger dragging.
      for (let v = 20; v < 200; v += 3) {
        await page.evaluate((val) => {
          const s = document.querySelector("#brightness");
          s.value = val;
          s.dispatchEvent(new Event("input", { bubbles: true }));
        }, v);
        await page.waitForTimeout(25);
      }
      await page.waitForTimeout(400);
      const posts = (await requests(t0)).filter((r) => r.method === "POST" && r.fields.brightness);
      const seconds = posts.length > 1 ? posts[posts.length - 1].t - posts[0].t : 1;
      const rate = posts.length / Math.max(seconds, 0.1);
      assert.ok(posts.length >= 8 && rate <= 12, `${posts.length} requests, ${rate.toFixed(1)}/s`);
      assert.strictEqual(posts[posts.length - 1].fields.brightness, "197", "last value must arrive");
      const state = await (await fetch(`${BASE}/api/state`)).json();
      assert.strictEqual(state.brightness, 197);
    });

    await check("colour picker sends live colours", async () => {
      const t0 = Date.now() / 1000;
      await page.evaluate(() => {
        const c = document.querySelector("#colour-a");
        c.value = "#00ff80";
        c.dispatchEvent(new Event("input", { bubbles: true }));
      });
      await page.waitForTimeout(300);
      const posts = (await requests(t0)).filter((r) => r.fields && r.fields.colourA);
      assert.strictEqual(posts.length, 1);
      assert.strictEqual(posts[0].fields.colourA, "#00ff80");
    });

    await check("save a preset with an awkward name, then recall it", async () => {
      await page.click("#add-preset");
      await page.fill("#preset-name", 'Movie "night" 🎬');
      await page.click("#preset-ok");
      await page.waitForFunction(() => document.querySelector("#presets .preset .name")?.textContent.includes("Movie"));
      assert.strictEqual(await page.locator("#presets .preset .name").first().textContent(), 'Movie "night" 🎬');
      await page.click('[data-mode="twinkle"]');
      await page.waitForFunction(() => document.querySelector('[data-mode="twinkle"]').getAttribute("aria-checked") === "true");
      await page.locator("#presets .preset .name").first().click();
      await page.waitForFunction(() => document.querySelector('[data-mode="spectrum-snappy"]').getAttribute("aria-checked") === "true");
    });

    await check("pressing Enter in the name box renames (never deletes)", async () => {
      await page.locator("#presets .preset").nth(0).locator(".btn").click();
      await page.fill("#preset-name", "Movie time");
      await page.press("#preset-name", "Enter");
      await page.waitForFunction(() => document.querySelector("#presets .preset .name").textContent === "Movie time");
      const state = await (await fetch(`${BASE}/api/state`)).json();
      assert.ok(state.presets[0].used, "preset still exists");
    });

    await check("deleting a preset asks first; Cancel keeps it", async () => {
      await page.locator("#presets .preset").nth(0).locator(".btn").click();
      await page.click("#preset-delete");
      await page.waitForSelector("#confirm-dialog[open]");
      await page.click('#confirm-dialog button[value="cancel"]');
      await page.waitForTimeout(200);
      assert.ok((await (await fetch(`${BASE}/api/state`)).json()).presets[0].used);
      await page.locator("#presets .preset").nth(0).locator(".btn").click();
      await page.click("#preset-delete");
      await page.click("#confirm-ok");
      await page.waitForFunction(() => document.querySelectorAll("#presets .preset").length === 0);
    });

    await check("when all 8 slots are used, the add button says so", async () => {
      for (let slot = 1; slot <= 8; slot++)
        await fetch(`${BASE}/api/preset/save`, { method: "POST", body: new URLSearchParams({ slot, name: `P${slot}` }) });
      await page.waitForFunction(() => document.querySelectorAll("#presets .preset").length === 8, null, { timeout: 4000 });
      assert.ok(await page.isDisabled("#add-preset"));
      await page.screenshot({ path: path.join(OUT, `${phone}-presets-full.png`), fullPage: true });
      for (let slot = 1; slot <= 8; slot++)
        await fetch(`${BASE}/api/preset/delete`, { method: "POST", body: new URLSearchParams({ slot }) });
      await page.waitForFunction(() => !document.querySelector("#add-preset").disabled, null, { timeout: 4000 });
    });

    await check("another phone's change shows up here within a few seconds", async () => {
      await fetch(`${BASE}/api/state`, { method: "POST", body: new URLSearchParams({ mode: "twinkle" }) });
      await page.waitForFunction(() => document.querySelector('[data-mode="twinkle"]').getAttribute("aria-checked") === "true",
                                 null, { timeout: 4000 });
    });

    await check("share: QR code and the page's address", async () => {
      await page.click("#share-btn");
      await page.waitForSelector("#share-dialog[open] #qr svg path");
      assert.strictEqual(await page.textContent("#share-url"), "http://192.168.3.234/");
      assert.ok((await page.textContent("#share-local")).includes("spectrum.local"));
      await page.screenshot({ path: path.join(OUT, `${phone}-share.png`) });
      await page.click('#share-dialog button[value="close"]');
    });

    await check("settings: status facts, and risky actions need confirming", async () => {
      await page.click("#settings-btn");
      await page.waitForSelector("#settings-view:not([hidden])");
      const facts = await page.textContent("#facts");
      assert.ok(facts.includes("Home Wi-Fi") && facts.includes("strong signal") && facts.includes("43%"), facts);
      const t0 = Date.now() / 1000;
      await page.click("#forget-btn");
      await page.waitForSelector("#confirm-dialog[open]");
      assert.ok((await page.textContent("#confirm-text")).includes("Spectrum-Setup"));
      await page.click('#confirm-dialog button[value="cancel"]');
      await page.waitForTimeout(200);
      assert.strictEqual((await requests(t0)).filter((r) => r.path.includes("forget")).length, 0,
                         "Cancel must not forget Wi-Fi");
      await page.click("#learn-btn");
      await page.click("#confirm-ok");
      await page.waitForFunction(() => document.querySelector("#facts").textContent.includes("Learning"));
      await page.screenshot({ path: path.join(OUT, `${phone}-settings.png`), fullPage: true });
      await page.click("#back-btn");
      await page.waitForSelector("#main-view:not([hidden])");
    });

    await check("firmware update: wrong files caught, a good one installs, the restart is followed", async () => {
      await page.click("#settings-btn");
      await page.waitForSelector("#settings-view:not([hidden])");
      const t0 = Date.now() / 1000;
      const file = (name, buffer) => ({ name, mimeType: "application/octet-stream", buffer });

      // The IDE's .merged.bin: caught on the page, nothing sent.
      await page.setInputFiles("#fw-file", file("SpectrumAnalyzer.ino.merged.bin", fakeFirmware(4000, true)));
      await page.waitForSelector("#fw-msg.bad");
      assert.ok((await page.textContent("#fw-msg")).includes(".merged.bin"));

      // Another sketch's firmware: sent, refused by the lights, their reason shown.
      await page.setInputFiles("#fw-file", file("WifiMatterSpike.ino.esp32s3.bin", fakeFirmware(200000, false)));
      await page.waitForSelector("#confirm-dialog[open]");
      await page.click("#confirm-ok");
      await page.waitForFunction(() => document.querySelector("#fw-msg").textContent.includes("another sketch"));
      assert.ok(await page.isVisible("#fw-choose"), "can choose again after a refusal");

      // The right file: confirm, progress, restart, new version.
      await page.setInputFiles("#fw-file", file("SpectrumAnalyzer.ino.esp32s3.bin", fakeFirmware(1500000, true)));
      await page.waitForSelector("#confirm-dialog[open]");
      assert.ok((await page.textContent("#confirm-text")).includes("SpectrumAnalyzer.ino.esp32s3.bin (1.4 MB)"));
      await page.click("#confirm-ok");
      await page.waitForFunction(() => /restarting/.test(document.querySelector("#fw-msg").textContent));
      await page.screenshot({ path: path.join(OUT, `${phone}-update-restarting.png`), fullPage: true });
      await page.waitForFunction(() => /Updated to version mock-\d/.test(document.querySelector("#fw-msg").textContent),
                                 null, { timeout: 15000 });
      assert.ok(!(await page.isVisible("#offline")), "no offline banner for the expected restart");
      assert.ok((await page.textContent("#facts")).includes("mock-"), "status shows the new version");
      const uploads = (await requests(t0)).filter((r) => r.path === "/api/update");
      assert.strictEqual(uploads.length, 2, "the .merged.bin is never sent");
      await page.screenshot({ path: path.join(OUT, `${phone}-update-done.png`), fullPage: true });
      await page.click("#back-btn");
      await page.waitForSelector("#main-view:not([hidden])");
    });

    await check("Apple/Google Home: code while pairing is open, paired apps listed, remove asks first", async () => {
      await page.click("#settings-btn");
      await page.waitForSelector("#settings-view:not([hidden])");
      // Unpaired, pairing open (as after first start): QR code and the code to type.
      await page.waitForSelector("#home-qr svg path");
      assert.strictEqual(await page.textContent("#home-code"), "3497-011-2332");
      assert.ok(!(await page.isVisible("#home-unpair-btn")), "nothing to remove yet");
      await page.screenshot({ path: path.join(OUT, `${phone}-home-pairing.png`), fullPage: true });

      // Apple Home pairs: the code goes away, Apple Home is listed.
      await fetch(`${BASE}/__pair`, { method: "POST", body: new URLSearchParams({ app: "Apple Home" }) });
      await page.waitForFunction(() => document.querySelector("#home-state").textContent === "Paired with Apple Home.",
                                 null, { timeout: 4000 });
      assert.ok(!(await page.isVisible("#home-pairing")), "no code once pairing has closed");
      assert.strictEqual(await page.textContent("#home-pair-btn"), "Allow another app to pair (15 minutes)");

      // Allow Google to pair, and it does.
      await page.click("#home-pair-btn");
      await page.waitForSelector("#home-qr svg path");
      await fetch(`${BASE}/__pair`, { method: "POST", body: new URLSearchParams({ app: "Google Home" }) });
      await page.waitForFunction(() => document.querySelector("#home-state").textContent ===
                                 "Paired with Apple Home and Google Home.", null, { timeout: 4000 });

      // Remove from all: Cancel keeps them; confirming removes them and pairing reopens.
      const t0 = Date.now() / 1000;
      await page.click("#home-unpair-btn");
      await page.waitForSelector("#confirm-dialog[open]");
      await page.click('#confirm-dialog button[value="cancel"]');
      await page.waitForTimeout(200);
      assert.strictEqual((await requests(t0)).filter((r) => r.path.includes("home-unpair")).length, 0);
      await page.click("#home-unpair-btn");
      await page.click("#confirm-ok");
      await page.waitForFunction(() => document.querySelector("#home-state").textContent.startsWith("Pairing is open"));
      assert.ok(await page.isVisible("#home-qr svg"));
      await page.click("#back-btn");
      await page.waitForSelector("#main-view:not([hidden])");
    });

    await check("no script errors", async () => {
      assert.deepStrictEqual(errors, []);
    });

    await page.screenshot({ path: path.join(OUT, `${phone}-main-light.png`), fullPage: true });
    await context.close();

    const dark = await openPage(browser, phone, "dark");
    await dark.page.screenshot({ path: path.join(OUT, `${phone}-main-dark.png`), fullPage: true });
    await dark.context.close();
  }

  console.log("\nOffline");
  {
    const { context, page } = await openPage(browser, PHONES[0]);
    await check("banner appears when the lights can't be reached, and goes when they're back", async () => {
      server.kill();
      await page.waitForSelector("#offline:not([hidden])", { timeout: 5000 });
      server = await startServer();
      await page.waitForSelector("#offline", { state: "hidden", timeout: 5000 });
    });
    await context.close();
  }

  await browser.close();
  server.kill();
  console.log(`\n${failures === 0 ? "ALL PASSED" : failures + " FAILED"}  (screenshots in test/build/web/)`);
  process.exit(failures === 0 ? 0 : 1);
})();
