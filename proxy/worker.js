/*
  ============================================================
  DAD DASHBOARD SPORTS PROXY
  Cloudflare Worker

  Purpose: ESPN's scoreboard API returns a huge, deeply-nested
  JSON payload (100KB+) meant for a website. An ESP32 cannot
  reliably parse that (hit memory limits, nesting-depth limits,
  and library buffer truncation trying). This Worker does the
  heavy lifting on a real server instead: it fetches ESPN's data,
  extracts only what the OLED dashboard needs, and returns a
  tiny flat JSON array — typically under 1KB total.

  Usage from the ESP32:
    GET https://YOUR-WORKER-NAME.workers.dev/?sport=baseball&league=mlb
    GET https://YOUR-WORKER-NAME.workers.dev/?sport=baseball&league=mlb&team=LAD

  The optional "team" param (ESPN abbreviation, e.g. LAD, LAL, IND)
  filters the response down to just that team's game for today, if
  any. Without "team", all of today's games are returned.

  Response shape (always flat, always small):
    {
      "games": [
        { "matchup": "NYY 5 - BOS 3", "status": "LIVE Top 7th" },
        { "matchup": "LAD @ SD", "status": "Starts 7:10 PM" },
        { "matchup": "CHC 2 - STL 1", "status": "FINAL" }
      ],
      "count": 3
    }

  When "team" is given and that team has no game today, returns:
    { "games": [], "count": 0 }

  Deployment (free, ~5 minutes):
    1. Go to https://dash.cloudflare.com/ and sign up/log in (free).
    2. Go to "Workers & Pages" in the left sidebar.
    3. Click "Create" -> "Create Worker".
    4. Give it a name, e.g. "dad-dashboard-sports".
    5. Click "Edit code" / "Deploy" to open the editor.
    6. Delete the placeholder code and paste this entire file in.
    7. Click "Save and Deploy".
    8. Your proxy URL will be:
       https://dad-dashboard-sports.YOUR-SUBDOMAIN.workers.dev
    9. Test it by pasting that URL (plus ?sport=baseball&league=mlb)
       into a browser. You should see the small JSON shown above.
  ============================================================
*/

export default {
  async fetch(request) {
    const url = new URL(request.url);

    // CORS header so this can also be tested from a browser easily.
    const corsHeaders = {
      "Access-Control-Allow-Origin": "*",
      "Content-Type": "application/json",
    };

    // ---- Read query params (with safe defaults) ----
    const sport = url.searchParams.get("sport") || "baseball";
    const league = url.searchParams.get("league") || "mlb";
    const dates = url.searchParams.get("dates"); // optional YYYYMMDD
    const teamFilter = url.searchParams.get("team"); // optional abbreviation, e.g. "LAD"

    // ---- Build the real ESPN URL ----
    let espnUrl = `https://site.api.espn.com/apis/site/v2/sports/${sport}/${league}/scoreboard`;
    if (dates) {
      espnUrl += `?dates=${dates}`;
    }

    try {
      const espnResponse = await fetch(espnUrl);

      if (!espnResponse.ok) {
        return new Response(
          JSON.stringify({ error: `ESPN returned HTTP ${espnResponse.status}`, games: [], count: 0 }),
          { status: 502, headers: corsHeaders }
        );
      }

      const data = await espnResponse.json();
      const events = data.events || [];

      // ---- Extract only what the OLED needs ----
      const games = [];
      for (const event of events) {
        const competition = event.competitions && event.competitions[0];
        if (!competition || !competition.competitors) continue;

        const competitors = competition.competitors;
        if (competitors.length < 2) continue;

        let home = null, away = null;
        for (const c of competitors) {
          if (c.homeAway === "home") home = c;
          else away = c;
        }
        if (!home || !away) continue;

        const homeAbbr = (home.team && home.team.abbreviation) || "???";
        const awayAbbr = (away.team && away.team.abbreviation) || "???";
        const state = event.status && event.status.type && event.status.type.state; // pre/in/post

        let matchup, status;

        if (state === "in") {
          const detail = (event.status.type.shortDetail) || "LIVE";
          matchup = `${awayAbbr} ${away.score || "0"} - ${homeAbbr} ${home.score || "0"}`;
          status = `LIVE ${detail}`;
        } else if (state === "post") {
          matchup = `${awayAbbr} ${away.score || "0"} - ${homeAbbr} ${home.score || "0"}`;
          status = "FINAL";
        } else {
          // "pre" - not started yet
          matchup = `${awayAbbr} @ ${homeAbbr}`;
          const isoDate = event.date || "";
          status = `Starts ${formatTime(isoDate)}`;
        }

        games.push({ matchup, status, state: state || "", homeAbbr, awayAbbr });
      }

      // If a specific team was requested, filter down to just their game(s).
      let filteredGames = games;
      if (teamFilter) {
        const wanted = teamFilter.toUpperCase();
        filteredGames = games.filter(
          (g) => g.homeAbbr.toUpperCase() === wanted || g.awayAbbr.toUpperCase() === wanted
        );
      }

      // Sort: live games first, then scheduled, then final
      const order = { in: 0, pre: 1, post: 2 };
      filteredGames.sort((a, b) => (order[a.state] ?? 3) - (order[b.state] ?? 3));

      // Strip internal fields before sending - the ESP32 doesn't need
      // them once filtering/sorting is done, keeps payload smaller.
      const slimGames = filteredGames.map(({ matchup, status }) => ({ matchup, status }));

      return new Response(
        JSON.stringify({ games: slimGames, count: slimGames.length }),
        { headers: corsHeaders }
      );
    } catch (err) {
      return new Response(
        JSON.stringify({ error: String(err), games: [], count: 0 }),
        { status: 500, headers: corsHeaders }
      );
    }
  },
};

// Converts an ESPN UTC ISO timestamp into a simple 12-hour Pacific
// time label. Fixed offset, not DST-aware - good enough for a
// "starts at" glance on a small OLED.
function formatTime(isoDate) {
  const match = isoDate.match(/T(\d{2}):(\d{2})/);
  if (!match) return "Scheduled";

  let hour = parseInt(match[1], 10);
  const minute = match[2];

  const UTC_OFFSET_HOURS = -7; // Pacific Daylight Time
  hour = (hour + UTC_OFFSET_HOURS + 24) % 24;

  const isPM = hour >= 12;
  let displayHour = hour % 12;
  if (displayHour === 0) displayHour = 12;

  return `${displayHour}:${minute} ${isPM ? "PM" : "AM"}`;
}
