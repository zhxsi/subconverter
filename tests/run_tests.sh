#!/bin/sh
# Comprehensive offline regression test suite for subconverter.
# Usage: sh tests/run_tests.sh [BINARY_PATH] [BASE_DIR]
set -eu

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${1:-$REPO_ROOT/build/subconverter}"
BASE_SRC="${2:-$REPO_ROOT/base}"
TPL_SRC="$REPO_ROOT/template.ini"

if [ ! -x "$BIN" ]; then
    echo "ERROR: subconverter binary not found or not executable: $BIN"
    exit 1
fi
if [ ! -d "$BASE_SRC" ]; then
    echo "ERROR: base directory not found: $BASE_SRC"
    exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'kill "${MOCK_PID:-}" "${SUB_PID:-}" "${SF_PID:-}" 2>/dev/null || true; rm -rf "$TMPDIR"' EXIT

assert_has() {
    text="$1"
    needle="$2"
    msg="$3"
    if ! printf '%s\n' "$text" | grep -Fq "$needle"; then
        echo "ASSERT FAILED: $msg (missing: $needle)"
        exit 1
    fi
}

assert_not_has() {
    text="$1"
    needle="$2"
    msg="$3"
    if printf '%s\n' "$text" | grep -Fq "$needle"; then
        echo "ASSERT FAILED: $msg (unexpected: $needle)"
        exit 1
    fi
}

check_clash_integrity() {
    file="$1"
    label="$2"
    if grep -qE "\{\{|\{%|\{#" "$file"; then
        echo "ERROR [$label]: Unrendered template tag!"
        exit 1
    fi
    awk -v label="$label" '
    /^proxies:/ { sec="proxies"; next }
    /^proxy-groups:/ { sec="groups"; next }
    /^rules:/ { sec="rules"; next }
    /^[^ ]/ { sec=""; next }
    sec == "proxies" && /^  - / {
        line = $0
        if (match(line, /name: [^,}]+/)) {
            p = substr(line, RSTART + 6, RLENGTH - 6)
            gsub(/^["'\'' ]+|["'\'' ]+$/, "", p)
            proxies[p] = 1
        }
    }
    sec == "groups" && /^  - name: / {
        g = substr($0, 11)
        gsub(/^["'\'' ]+|["'\'' ]+$/, "", g)
        groups[g] = 1
        cur_g = g
        in_p = 0
        next
    }
    sec == "groups" && /^    proxies:/ { in_p = 1; next }
    sec == "groups" && /^    [a-z]/ { in_p = 0; next }
    sec == "groups" && in_p && /^      - / {
        ref = substr($0, 9)
        gsub(/^["'\'' ]+|["'\'' ]+$/, "", ref)
        group_refs[cur_g, ++group_ref_cnt[cur_g]] = ref
        next
    }
    sec == "rules" && /^  - / {
        line = substr($0, 5)
        n = split(line, parts, ",")
        target = parts[n]
        if (target == "no-resolve" && n >= 2) target = parts[n-1]
        rule_targets[++rcnt] = target
        rule_lines[rcnt] = line
    }
    END {
        builtins["DIRECT"] = 1
        builtins["REJECT"] = 1
        builtins["REJECT-DROP"] = 1
        builtins["PASS"] = 1
        builtins["COMPATIBLE"] = 1
        builtins["GLOBAL"] = 1
        bad = 0
        for (g in groups) {
            if (group_ref_cnt[g] + 0 == 0) {
                print "ERROR [" label "]: Empty group: " g
                bad = 1
            }
            for (i = 1; i <= group_ref_cnt[g]; i++) {
                r = group_refs[g, i]
                if (!(r in proxies) && !(r in groups) && !(r in builtins)) {
                    print "ERROR [" label "]: Dangling proxy ref \"" r "\" in group \"" g "\""
                    bad = 1
                }
            }
        }
        for (i = 1; i <= rcnt; i++) {
            t = rule_targets[i]
            if (!(t in groups) && !(t in builtins)) {
                print "ERROR [" label "]: Dangling rule target \"" t "\" in rule \"" rule_lines[i] "\""
                bad = 1
            }
        }
        if (bad) exit 1
        print "[" label "] YAML integrity OK (proxies=" length(proxies) ", groups=" length(groups) ", rules=" rcnt ")"
    }
    ' "$file"
}

get_group_block() {
    file="$1"
    gname="$2"
    awk -v target="$gname" '
    /^proxy-groups:/ { in_pg=1; next }
    /^rules:/ { in_pg=0; next }
    in_pg && /^  - name: / {
        g = substr($0, 11)
        show = (g == target)
    }
    in_pg && show { print }
    ' "$file"
}

# Validate template.ini static invariants
assert_not_has "$(cat "$TPL_SRC")" "gstatic.com" "template.ini must not reference gstatic.com"
assert_has "$(cat "$TPL_SRC")" "https://cp.cloudflare.com/generate_204" "template.ini must use Cloudflare speedtest URL"
assert_has "$(cat "$TPL_SRC")" "exclude_remarks=" "template.ini must include default info-node exclude_remarks"

# Prepare isolated runtime workspace
mkdir -p "$TMPDIR/work/base"
cp -r "$BASE_SRC"/* "$TMPDIR/work/base/"
mkdir -p "$TMPDIR/work/base/base"

cat > "$TMPDIR/work/base/pref.ini" <<'EOF'
[common]
api_mode=false
api_access_token=testtoken
default_url=
enable_insert=false
log_level=info
base_path=base
enable_rule_generator=true
overwrite_original_rules=true
clash_rule_base=base/mini_clash_base.yml
surge_rule_base=base/all_base.tpl
singbox_rule_base=base/all_base.tpl
[userinfo]
stream_rule=^剩余流量：(.*?)\|总流量：(.*)$|total=$2&left=$1
time_rule=^过期时间：(\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)$|$1:$2:$3:$4:$5:$6
[clash]
clash_proxies_style=flow
clash_proxy_groups_style=block
[server]
listen=127.0.0.1
port=25590
[advanced]
print_debug_info=false
max_concurrent_threads=8
enable_cache=false
async_fetch_ruleset=true
EOF

cat > "$TMPDIR/work/base/base/mini_clash_base.yml" <<'EOF'
port: 7890
mode: rule
EOF

cat > "$TMPDIR/work/base/base/China.list" <<'EOF'
DOMAIN-SUFFIX,china-normal.example.cn
EOF

cat > "$TMPDIR/work/base/base/MAXChina.list" <<'EOF'
DOMAIN-SUFFIX,china-max.example.cn
EOF

# Offline mirror of template.ini
sed 's|https://raw.githubusercontent.com/blackmatrix7/ios_rule_script/master/rule/Clash/China/China.list|base/China.list|g; s|https://raw.githubusercontent.com/blackmatrix7/ios_rule_script/master/rule/Clash/China/MAXChina.list|base/MAXChina.list|g; /https:\/\/raw\.githubusercontent\.com/d' "$TPL_SRC" > "$TMPDIR/work/base/base/template_offline.ini"

# Suite 1 fixture: VLESS + REALITY + rule dedup + compound AND rule
cat <<'EOF' | base64 -w 0 > "$TMPDIR/work/base/base/sub_vless.sub"
vless://11111111-1111-1111-1111-111111111111@1.1.1.1:443?encryption=none&security=reality&sni=www.apple.com&fp=chrome&pbk=PUBKEY_RAW&sid=01234567&type=raw&flow=xtls-rprx-vision&alpn=h2%2Chttp%2F1.1#US-Reality-Raw
vless://22222222-2222-2222-2222-222222222222@1.1.1.2:443?encryption=none&security=reality&sni=www.icloud.com&fp=firefox&pbk=PUBKEY_DEF&sid=0089&flow=xtls-rprx-vision#JP-Reality-Default
vless://33333333-3333-3333-3333-333333333333@1.1.1.3:443?encryption=none&security=tls&sni=ws.example.com&fp=safari&type=httpupgrade&host=cdn.example.com&path=%2Fws%3Fed%3D2048#SG-HTTPUpgrade
EOF

cat > "$TMPDIR/work/base/base/tpl_dedup.ini" <<'EOF'
[custom]
ruleset=Proxy,[]AND,((DOMAIN,a.example.com),(DST-PORT,443))
ruleset=DIRECT,[]AND,((DOMAIN,a.example.com),(DST-PORT,443))
ruleset=Proxy,[]DOMAIN,dup.example.com
ruleset=DIRECT,[]DOMAIN,dup.example.com
ruleset=Proxy,[]FINAL
custom_proxy_group=Proxy`select`.*
enable_rule_generator=true
overwrite_original_rules=true
enable_rule_dedup=true
prune_empty_groups=true
EOF

# Suite 2 & 3 fixture: Broad->narrow regions + airport info nodes + traps + restricted regions
cat <<'EOF' | base64 -w 0 > "$TMPDIR/work/base/base/sub_full.sub"
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.1:443#剩余流量：50GB|总流量：100GB
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.2:443#过期时间：2028-01-01 00:00:00
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.3:443#最新网址：example.org
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.4:443#香港-01
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.5:443#日本-东京
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.6:443#美国-洛杉矶-CN2
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.7:443#印度-孟买
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.8:443#瑞士-苏黎世-CN2-by-AWS
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.9:443#瑞典-CU9929
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.10:443#欧洲专线-01
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.11:443#巴西-圣保罗
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.16:443#智利-圣地亚哥
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.12:443#缅甸-仰光
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.13:443#俄罗斯-莫斯科
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.14:443#中国-回国
ss://YWVzLTI1Ni1nY206cGFzcw@1.1.1.15:443#澳门-01
EOF

cat <<'EOF' | base64 -w 0 > "$TMPDIR/work/base/base/sub_restricted.sub"
ss://YWVzLTI1Ni1nY206cGFzcw@2.2.2.1:443#香港-01
ss://YWVzLTI1Ni1nY206cGFzcw@2.2.2.2:443#俄罗斯-莫斯科
ss://YWVzLTI1Ni1nY206cGFzcw@2.2.2.3:443#中国-回国
ss://YWVzLTI1Ni1nY206cGFzcw@2.2.2.4:443#澳门-01
EOF

cat <<'EOF' | base64 -w 0 > "$TMPDIR/work/base/base/sub_surge_vless.sub"
ss://YWVzLTI1Ni1nY206cGFzcw@3.3.3.1:443#香港-SS
vless://11111111-1111-1111-1111-111111111111@3.3.3.2:443?encryption=none&security=reality&sni=www.apple.com&fp=chrome&pbk=abc123&sid=01ab&type=tcp&flow=xtls-rprx-vision#美国-VLESS
EOF

cat <<'EOF' | base64 -w 0 > "$TMPDIR/work/base/base/sub_fold.sub"
ss://YWVzLTI1Ni1nY206cGFzcw@4.4.4.1:443#香港-01
ss://YWVzLTI1Ni1nY206cGFzcw@4.4.4.2:443#日本-东京
ss://YWVzLTI1Ni1nY206cGFzcw@4.4.4.3:443#美国-洛杉矶
EOF

cat > "$TMPDIR/work/base/base/tpl_hierarchy.ini" <<'EOF'
[custom]
ruleset=Proxy,[]FINAL
custom_proxy_group=Proxy`select`[]EAST_ASIA_SAFE`[]NORTHERN_EUROPE`[]SOUTH_AMERICA`[]CH_ONLY`[]IN_ONLY`[]BR_ONLY
custom_proxy_group=EAST_ASIA_SAFE`url-test`!!REGION=EAST_ASIA,!CN,!HK,!MO`https://cp.cloudflare.com/generate_204`300,5,50
custom_proxy_group=NORTHERN_EUROPE`url-test`!!REGION=NORTHERN_EUROPE`https://cp.cloudflare.com/generate_204`300,5,50
custom_proxy_group=SOUTH_AMERICA`url-test`!!REGION=SOUTH_AMERICA`https://cp.cloudflare.com/generate_204`300,5,50
custom_proxy_group=CH_ONLY`url-test`!!REGION=CH`https://cp.cloudflare.com/generate_204`300,5,50
custom_proxy_group=IN_ONLY`url-test`!!REGION=IN`https://cp.cloudflare.com/generate_204`300,5,50
custom_proxy_group=BR_ONLY`url-test`!!REGION=BR`https://cp.cloudflare.com/generate_204`300,5,50
enable_rule_generator=true
overwrite_original_rules=true
prune_empty_groups=true
EOF

cd "$TMPDIR/work"
"$BIN" -f "$TMPDIR/work/base/pref.ini" >"$TMPDIR/work/server.log" 2>&1 &
SUB_PID=$!

for _ in $(seq 1 40); do
    if curl -fsS http://127.0.0.1:25590/version >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

echo "=== Suite 1: Mihomo VLESS + REALITY & Rule Dedup ==="
curl -fsS -o "$TMPDIR/s1.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&udp=false&config=base/tpl_dedup.ini&url=base/sub_vless.sub"
check_clash_integrity "$TMPDIR/s1.yaml" "Suite1-VLESS"
s1="$(cat "$TMPDIR/s1.yaml")"
assert_has "$s1" "US-Reality-Raw" "type=raw parsed"
assert_has "$s1" "JP-Reality-Default" "omitted type parsed"
assert_has "$s1" "SG-HTTPUpgrade" "httpupgrade parsed"
assert_has "$s1" 'short-id: "01234567"' "numeric short-id quoted"
assert_has "$s1" 'short-id: "0089"' "leading-zero short-id quoted"
assert_has "$s1" "client-fingerprint: chrome" "client-fingerprint exported"
assert_has "$s1" "udp: false" "udp=false respected"
assert_has "$s1" "alpn: [h2, http/1.1]" "multi-value alpn decoded"
assert_has "$s1" "AND,((DOMAIN,a.example.com),(DST-PORT,443)),Proxy" "compound AND rule intact"
assert_not_has "$s1" "AND,((DOMAIN,a.example.com),(DST-PORT,443)),DIRECT" "duplicate AND rule deduped"
assert_not_has "$s1" "DOMAIN,dup.example.com,DIRECT" "duplicate DOMAIN rule deduped"

curl -fsS -o "$TMPDIR/s1_list.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&list=true&url=base/sub_vless.sub"
assert_has "$(cat "$TMPDIR/s1_list.yaml")" 'short-id: "0089"' "list=true short-id quoted"
echo "Suite 1 PASSED!"

echo "=== Suite 2: Region Hierarchy + Single AI Macros + Info-Node Exclusion + AI Stability ==="
curl -fsS -D "$TMPDIR/s2_hdr.txt" -o "$TMPDIR/s2.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&config=base/template_offline.ini&url=base/sub_full.sub"
check_clash_integrity "$TMPDIR/s2.yaml" "Suite2-RegionAI"
s2="$(cat "$TMPDIR/s2.yaml")"

# Info nodes filtered out, yet Subscription-UserInfo still extracted!
assert_not_has "$s2" "剩余流量" "Info node filtered out"
assert_not_has "$s2" "过期时间" "Expiry info node filtered out"
assert_not_has "$s2" "最新网址" "Website info node filtered out"
assert_has "$(cat "$TMPDIR/s2_hdr.txt")" "Subscription-UserInfo:" "Subscription-UserInfo extracted before filtering"

# Cloudflare speedtest URL only
assert_not_has "$s2" "gstatic.com" "Zero gstatic URLs"
assert_has "$s2" "https://cp.cloudflare.com/generate_204" "Cloudflare speedtest URL used"

# AI default-first fallback + auto-select (tolerance 150, interval 600)
blk_claude_sel="$(get_group_block "$TMPDIR/s2.yaml" "🟠 Claude")"
first_claude_ref="$(printf '%s\n' "$blk_claude_sel" | awk '/^      - /{print substr($0, 9); exit}')"
if [ "$first_claude_ref" != "🛡️ Claude故障转移" ]; then
    echo "ASSERT FAILED: Claude default should be fallback first, got: $first_claude_ref"
    exit 1
fi
blk_claude_auto="$(get_group_block "$TMPDIR/s2.yaml" "⚡ Claude自动选择")"
assert_has "$blk_claude_auto" "interval: 600" "AI auto-select interval=600"
assert_has "$blk_claude_auto" "tolerance: 150" "AI auto-select tolerance=150"
assert_not_has "$blk_claude_auto" "缅甸-仰光" "Claude excludes MM"
assert_not_has "$blk_claude_auto" "香港-01" "Claude excludes HK"
assert_has "$blk_claude_auto" "巴西-圣保罗" "Claude includes BR"
assert_has "$blk_claude_auto" "瑞士-苏黎世-CN2-by-AWS" "Claude includes CH (CN2/by trap safe)"
assert_has "$blk_claude_auto" "瑞典-CU9929" "Claude includes SE (CU9929 trap safe)"

# Continents: Asia/Europe/America kept; Oceania/Africa pruned
s2_groups="$(grep "^  - name: " "$TMPDIR/s2.yaml")"
for c in "🌏 亚洲节点" "🇪🇺 欧洲节点" "🌎 美洲节点"; do
    assert_has "$s2_groups" "  - name: $c" "Continent $c kept"
done
for c in "🌊 大洋洲节点" "🌍 非洲节点"; do
    assert_not_has "$s2_groups" "  - name: $c" "Empty continent $c pruned"
done

# Sub-regions & single non-primary countries
curl -fsS -o "$TMPDIR/s2h.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&config=base/tpl_hierarchy.ini&url=base/sub_full.sub"
check_clash_integrity "$TMPDIR/s2h.yaml" "Suite2-SubRegions"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "EAST_ASIA_SAFE")" "      - 日本-东京" "EAST_ASIA_SAFE has JP"
assert_not_has "$(get_group_block "$TMPDIR/s2h.yaml" "EAST_ASIA_SAFE")" "香港-01" "EAST_ASIA_SAFE excludes HK"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "NORTHERN_EUROPE")" "      - 瑞典-CU9929" "NORTHERN_EUROPE has SE"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "SOUTH_AMERICA")" "      - 巴西-圣保罗" "SOUTH_AMERICA has BR"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "SOUTH_AMERICA")" "      - 智利-圣地亚哥" "SOUTH_AMERICA has CL"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "BR_ONLY")" "      - 巴西-圣保罗" "BR_ONLY has BR"
assert_not_has "$(get_group_block "$TMPDIR/s2h.yaml" "BR_ONLY")" "智利-圣地亚哥" "BR_ONLY excludes CL"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "CH_ONLY")" "      - 瑞士-苏黎世-CN2-by-AWS" "CH_ONLY has CH"
assert_has "$(get_group_block "$TMPDIR/s2h.yaml" "IN_ONLY")" "      - 印度-孟买" "IN_ONLY has IN"
echo "Suite 2 PASSED!"

echo "=== Suite 3: Empty-Group Pruning (Clash / Surge 4 / sing-box) & China Toggle ==="
curl -fsS -o "$TMPDIR/s3.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&config=base/template_offline.ini&url=base/sub_restricted.sub"
check_clash_integrity "$TMPDIR/s3.yaml" "Suite3-Restricted"
s3_groups="$(grep "^  - name: " "$TMPDIR/s3.yaml")"
for ai_g in "🌀 ChatGPT" "🛡️ ChatGPT故障转移" "⚡ ChatGPT自动选择" "💻 Codex" "🟠 Claude" "✨ Gemini" "⚫ Grok"; do
    assert_not_has "$s3_groups" "  - name: $ai_g" "Pruned empty AI group $ai_g"
done

# China vs MAXChina toggle
assert_has "$s2" "china-normal.example.cn" "Default China.list"
curl -fsS -o "$TMPDIR/s3_max.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&config=base/template_offline.ini&url=base/sub_full.sub&china=max"
assert_has "$(cat "$TMPDIR/s3_max.yaml")" "china-max.example.cn" "china=max uses MAXChina.list"

# Surge 4 target-specific pruning (skips VLESS -> US & AI pruned)
curl -fsS -o "$TMPDIR/s3_surge.conf" "http://127.0.0.1:25590/sub?target=surge&ver=4&config=base/template_offline.ini&url=base/sub_surge_vless.sub"
s3_surge="$(cat "$TMPDIR/s3_surge.conf")"
assert_has "$s3_surge" "🇭🇰 香港节点 =" "Surge keeps HK"
assert_not_has "$s3_surge" "🇺🇸 美国节点 =" "Surge prunes US after skipping VLESS"
assert_not_has "$s3_surge" "🌀 ChatGPT =" "Surge prunes ChatGPT"

# sing-box pruning
curl -fsS -o "$TMPDIR/s3_sb.json" "http://127.0.0.1:25590/sub?target=singbox&config=base/template_offline.ini&url=base/sub_restricted.sub"
s3_sb="$(cat "$TMPDIR/s3_sb.json")"
assert_has "$s3_sb" '"tag":"🇭🇰 香港节点"' "sing-box keeps HK"
assert_not_has "$s3_sb" '"tag":"🌀 ChatGPT"' "sing-box prunes ChatGPT"

# Single-country continent folding (America has only US -> folded; Asia has HK+JP -> kept)
curl -fsS -o "$TMPDIR/s3_fold.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&config=base/template_offline.ini&url=base/sub_fold.sub"
check_clash_integrity "$TMPDIR/s3_fold.yaml" "Suite3-ContinentFold"
s3_fold_groups="$(grep "^  - name: " "$TMPDIR/s3_fold.yaml")"
assert_has "$s3_fold_groups" "  - name: 🌏 亚洲节点" "Multi-country Asia continent kept"
assert_has "$s3_fold_groups" "  - name: 🇺🇸 美国节点" "US country group kept"
assert_not_has "$s3_fold_groups" "  - name: 🌎 美洲节点" "Single-country America continent folded into US"

# Server-side Gzip response compression (compiled with ZLIB/CPPHTTPLIB_ZLIB_SUPPORT)
curl -fsS -D "$TMPDIR/s3_gz_hdr.txt" --compressed -o "$TMPDIR/s3_gz.yaml" "http://127.0.0.1:25590/sub?target=clash&new_name=true&config=base/template_offline.ini&url=base/sub_full.sub"
diff -q "$TMPDIR/s2.yaml" "$TMPDIR/s3_gz.yaml"
assert_has "$(cat "$TMPDIR/s3_gz_hdr.txt")" "Content-Encoding: gzip" "Server returned gzip-compressed response"
echo "Server Gzip compression: ENABLED and verified byte-identical"
echo "Suite 3 PASSED!"

kill "$SUB_PID" 2>/dev/null || true
wait "$SUB_PID" 2>/dev/null || true
SUB_PID=""

# Suites 4 & 5 require g++ and repo include/httplib.h for the local mock HTTP upstream
if command -v g++ >/dev/null 2>&1 && [ -f "$REPO_ROOT/include/httplib.h" ]; then
    echo "=== Suites 4 & 5: Pipelined Concurrency + Singleflight Cold Cache + Cache Fallback + Gzip ==="
    cat > "$TMPDIR/mock_server.cpp" <<'EOF'
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#define CPPHTTPLIB_LISTEN_BACKLOG 1024
#include "httplib.h"

int main() {
    httplib::Server svr;
    svr.new_task_queue = [] { return new httplib::ThreadPool(64); };
    std::atomic<int> sf_hits{0};
    std::atomic<bool> fail_sf{false};
    std::atomic<bool> saw_accept_encoding{false};
    std::atomic<bool> saw_get_content_type{false};

    struct SubItem { const char *path; const char *b64; const char *userinfo; };
    const SubItem subs[] = {
        {"/sub1", "c3M6Ly9ZV1Z6TFRJMU5pMW5ZMjA2Y0dGemN3QDEwLjAuMC4xOjQ0MyNTdWIxLVVTCg==", ""},
        {"/sub2", "c3M6Ly9ZV1Z6TFRJMU5pMW5ZMjA2Y0dGemN3QDEwLjAuMC4yOjQ0MyNTdWIyLUpQCg==", ""},
        {"/sub3", "c3M6Ly9ZV1Z6TFRJMU5pMW5ZMjA2Y0dGemN3QDEwLjAuMC4zOjQ0MyNTdWIzLVNHCg==", "upload=100; download=200; total=1000; expire=1800000000"},
        {"/sub4", "c3M6Ly9ZV1Z6TFRJMU5pMW5ZMjA2Y0dGemN3QDEwLjAuMC40OjQ0MyNTdWI0LVVLCg==", ""},
        {"/sub5", "c3M6Ly9ZV1Z6TFRJMU5pMW5ZMjA2Y0dGemN3QDEwLjAuMC41OjQ0MyNTdWI1LUhLCg==", "upload=500; download=600; total=2000; expire=1900000000"},
    };
    for (const auto &s : subs) {
        svr.Get(s.path, [s, &saw_accept_encoding, &saw_get_content_type](const httplib::Request &req, httplib::Response &res) {
            if (req.has_header("Accept-Encoding") && !req.get_header_value("Accept-Encoding").empty())
                saw_accept_encoding.store(true);
            if (req.has_header("Content-Type"))
                saw_get_content_type.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            if (s.userinfo[0] != '\0')
                res.set_header("Subscription-UserInfo", s.userinfo);
            res.set_content(s.b64, "text/plain");
        });
    }
    svr.Get("/fail", [](const httplib::Request &, httplib::Response &res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        res.status = 500;
        res.set_content("upstream error", "text/plain");
    });
    for (int i = 1; i <= 4; ++i) {
        std::string p = "/rule" + std::to_string(i);
        std::string body = "DOMAIN-SUFFIX,rule" + std::to_string(i) + ".example.com\n";
        svr.Get(p, [body](const httplib::Request &, httplib::Response &res) {
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            res.set_content(body, "text/plain");
        });
    }
    svr.Get("/rule_sf", [&sf_hits, &fail_sf](const httplib::Request &, httplib::Response &res) {
        sf_hits.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        if (fail_sf.load()) {
            res.status = 500;
            res.set_content("down", "text/plain");
            return;
        }
        res.set_content("DOMAIN-SUFFIX,singleflight.example.com\n", "text/plain");
    });
    svr.Get("/sf_stats", [&sf_hits, &saw_accept_encoding, &saw_get_content_type](const httplib::Request &, httplib::Response &res) {
        res.set_content("hits=" + std::to_string(sf_hits.load()) + " ae=" + (saw_accept_encoding.load() ? "1" : "0") + " get_ct=" + (saw_get_content_type.load() ? "1" : "0") + "\n", "text/plain");
    });
    svr.Get("/sf_fail_on", [&fail_sf](const httplib::Request &, httplib::Response &res) {
        fail_sf.store(true);
        res.set_content("ok", "text/plain");
    });
    svr.Get("/remote_base.yml", [](const httplib::Request &, httplib::Response &res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        res.set_content("port: 7890\nmode: rule\n", "text/plain");
    });
    svr.Get("/remote_tpl.ini", [](const httplib::Request &, httplib::Response &res) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        res.set_content(
            "[custom]\n"
            "clash_rule_base=http://127.0.0.1:25591/remote_base.yml\n"
            "ruleset=Proxy,http://127.0.0.1:25591/rule1\n"
            "ruleset=Proxy,http://127.0.0.1:25591/rule2\n"
            "ruleset=Proxy,http://127.0.0.1:25591/rule3\n"
            "ruleset=Proxy,http://127.0.0.1:25591/rule4\n"
            "ruleset=Proxy,[]FINAL\n"
            "custom_proxy_group=Proxy`select`.*\n"
            "enable_rule_generator=true\n"
            "overwrite_original_rules=true\n"
            "prune_empty_groups=true\n",
            "text/plain");
    });
    svr.Get("/health", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("ok", "text/plain");
    });
    svr.listen("127.0.0.1", 25591);
    return 0;
}
EOF

    g++ -O2 -std=c++20 -pthread -I"$REPO_ROOT/include" "$TMPDIR/mock_server.cpp" -o "$TMPDIR/mock_server"
    "$TMPDIR/mock_server" &
    MOCK_PID=$!
    for _ in $(seq 1 30); do
        if curl -fsS http://127.0.0.1:25591/health >/dev/null 2>&1; then break; fi
        sleep 0.1
    done

    cat > "$TMPDIR/work/base/pref_sf.ini" <<'EOF'
[common]
api_mode=false
api_access_token=testtoken
default_url=
enable_insert=false
log_level=info
base_path=base
enable_rule_generator=true
overwrite_original_rules=true
clash_rule_base=base/mini_clash_base.yml
[clash]
clash_proxies_style=flow
clash_proxy_groups_style=block
[server]
listen=127.0.0.1
port=25592
[advanced]
print_debug_info=false
max_concurrent_threads=8
enable_cache=true
cache_subscription=0
cache_config=0
cache_ruleset=1
serve_cache_on_fetch_fail=true
async_fetch_ruleset=true
EOF

    cat > "$TMPDIR/work/base/base/tpl_sf.ini" <<'EOF'
[custom]
ruleset=Proxy,http://127.0.0.1:25591/rule_sf
ruleset=Proxy,[]FINAL
custom_proxy_group=Proxy`select`.*
enable_rule_generator=true
overwrite_original_rules=true
prune_empty_groups=true
EOF

    cat > "$TMPDIR/work/base/base/tpl_pipe.ini" <<'EOF'
[custom]
clash_rule_base=http://127.0.0.1:25591/remote_base.yml
ruleset=Proxy,http://127.0.0.1:25591/rule1
ruleset=Proxy,http://127.0.0.1:25591/rule2
ruleset=Proxy,http://127.0.0.1:25591/rule3
ruleset=Proxy,http://127.0.0.1:25591/rule4
ruleset=Proxy,[]FINAL
custom_proxy_group=Proxy`select`.*
enable_rule_generator=true
overwrite_original_rules=true
prune_empty_groups=true
EOF

    cd "$TMPDIR/work"
    "$BIN" -f "$TMPDIR/work/base/pref_sf.ini" >"$TMPDIR/work/server_sf.log" 2>&1 &
    SF_PID=$!
    for _ in $(seq 1 40); do
        if curl -fsS http://127.0.0.1:25592/version >/dev/null 2>&1; then break; fi
        sleep 0.1
    done

    # Suite 4: Pipelined concurrency (remote config + 6 subs + 4 rulesets + remote base)
    URLS="http%3A%2F%2F127.0.0.1%3A25591%2Fsub1%7Chttp%3A%2F%2F127.0.0.1%3A25591%2Fsub2%7Chttp%3A%2F%2F127.0.0.1%3A25591%2Fsub3%7Chttp%3A%2F%2F127.0.0.1%3A25591%2Ffail%7Chttp%3A%2F%2F127.0.0.1%3A25591%2Fsub4%7Chttp%3A%2F%2F127.0.0.1%3A25591%2Fsub5"
    T4_1=$(curl -fsS -D "$TMPDIR/s4_h1.txt" -o "$TMPDIR/s4_1.yaml" -w "%{time_total}" "http://127.0.0.1:25592/sub?target=clash&new_name=true&config=base/tpl_pipe.ini&url=${URLS}")
    MS4_1=$(awk -v t="$T4_1" 'BEGIN { printf "%d", t * 1000 }')
    echo "Suite 4a (11 x 800ms = 8800ms seq): ${MS4_1} ms"
    if [ "$MS4_1" -ge 1600 ]; then
        echo "ASSERT FAILED: Pipelined single-wave fetch too slow (${MS4_1} ms)"
        exit 1
    fi
    ORDER="$(awk '/^proxies:/{p=1;next} /^proxy-groups:/{p=0} p && /name:/ { if(match($0, /name: [^,}]+/)) print substr($0, RSTART+6, RLENGTH-6) }' "$TMPDIR/s4_1.yaml" | tr '\n' ',')"
    if [ "$ORDER" != "Sub1-US,Sub2-JP,Sub3-SG,Sub4-UK,Sub5-HK," ]; then
        echo "ASSERT FAILED: Node order mismatch: $ORDER"
        exit 1
    fi
    assert_has "$(cat "$TMPDIR/s4_h1.txt")" "upload=500; download=600; total=2000; expire=1900000000" "Deterministic UserInfo"

    sleep 2 # expire 1s ruleset cache so Wave 2 actually fetches fresh
    T4_2=$(curl -fsS -o "$TMPDIR/s4_2.yaml" -w "%{time_total}" "http://127.0.0.1:25592/sub?target=clash&new_name=true&config=http%3A%2F%2F127.0.0.1%3A25591%2Fremote_tpl.ini&url=${URLS}")
    MS4_2=$(awk -v t="$T4_2" 'BEGIN { printf "%d", t * 1000 }')
    echo "Suite 4b (remote config + 6 subs overlapped + 4 rulesets + remote base = 9600ms seq): ${MS4_2} ms"
    if [ "$MS4_2" -ge 2400 ]; then
        echo "ASSERT FAILED: Remote config + subs overlap too slow (${MS4_2} ms)"
        exit 1
    fi
    diff -q "$TMPDIR/s4_1.yaml" "$TMPDIR/s4_2.yaml"
    echo "Suite 4 PASSED!"

    # Suite 5: Singleflight cold-cache coalescing + Accept-Encoding + serve_cache_on_fetch_fail
    PIDS=""
    for i in 1 2 3 4 5 6; do
        ( curl -fsS -o "$TMPDIR/sf_${i}.yaml" "http://127.0.0.1:25592/sub?target=clash&new_name=true&config=base/tpl_sf.ini&url=base/sub_restricted.sub" ) &
        PIDS="$PIDS $!"
    done
    for pid in $PIDS; do wait "$pid"; done
    for i in 1 2 3 4 5 6; do
        assert_has "$(cat "$TMPDIR/sf_${i}.yaml")" "singleflight.example.com" "Singleflight client $i got ruleset"
    done
    STATS="$(curl -fsS http://127.0.0.1:25591/sf_stats)"
    echo "Singleflight stats after 6 simultaneous cold clients: $STATS"
    assert_has "$STATS" "hits=1 " "Singleflight coalesced 6 cold requests into exactly 1 upstream fetch"
    assert_has "$STATS" "ae=1" "libcurl advertised Accept-Encoding compression"
    assert_has "$STATS" "get_ct=0" "GET requests do not send Content-Type: application/json"

    # Flip upstream /rule_sf to HTTP 500 and wait for 1s cache TTL expiry -> fallback to cached content!
    curl -fsS http://127.0.0.1:25591/sf_fail_on >/dev/null
    sleep 2
    curl -fsS -o "$TMPDIR/sf_fallback.yaml" "http://127.0.0.1:25592/sub?target=clash&new_name=true&config=base/tpl_sf.ini&url=base/sub_restricted.sub"
    assert_has "$(cat "$TMPDIR/sf_fallback.yaml")" "singleflight.example.com" "serve_cache_on_fetch_fail served stale cache on HTTP 500"
    echo "Suite 5 PASSED!"
fi

echo "=== ALL REGRESSION TESTS PASSED! ==="
