#ifndef REGION_H_INCLUDED
#define REGION_H_INCLUDED

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "utils/string.h"

struct RegionPreset
{
    std::string pattern;
    bool is_region = true;
};

using RegionPresetMap = std::map<std::string, RegionPreset>;

struct RegionRule
{
    std::string name;
    std::string pattern;
    bool append = false;
    bool is_region = true;
};

using RegionRuleConfigs = std::vector<RegionRule>;

inline std::string normalizeRegionName(const std::string &raw)
{
    return toUpper(trim(raw));
}

inline const RegionPresetMap &getBuiltinRegionPresets()
{
    static const RegionPresetMap presets = {
        // 15 primary country/territory presets (is_region=true; excluded by default OTHER)
        {"US", {R"(美国|美國|United[ _-]*States|(?<![A-Za-z])(?:US|USA)(?![A-Za-z])|洛杉矶|洛杉磯|圣何塞|聖何塞|西雅图|西雅圖|芝加哥|纽约|紐約|旧金山|舊金山|达拉斯|達拉斯|Los[ _-]*Angeles|San[ _-]*Jose|Seattle|Chicago|New[ _-]*York|San[ _-]*Francisco|Dallas|(?<![A-Za-z])(?:LAX|SJC|SEA|JFK|SFO|DFW|ORD)(?![A-Za-z])|🇺🇸)", true}},
        {"HK", {R"(香港|Hong[ _-]*Kong|(?<![A-Za-z])(?:HK|HKG)(?![A-Za-z])|🇭🇰)", true}},
        {"JP", {R"(日本|东京|東京|大阪|Tokyo|Osaka|Japan|(?<![A-Za-z])(?:JP|JPN|NRT|HND|KIX)(?![A-Za-z])|🇯🇵)", true}},
        {"SG", {R"(新加坡|狮城|獅城|Singapore|(?<![A-Za-z])(?:SG|SGP|SIN)(?![A-Za-z])|🇸🇬)", true}},
        {"TW", {R"(台湾|台灣|台北|高雄|Taiwan|Taipei|Kaohsiung|(?<![A-Za-z])(?:TW|TWN|TPE|KHH)(?![A-Za-z])|🇹🇼)", true}},
        {"KR", {R"(韩国|韓国|南韩|南韓|首尔|首爾|釜山|仁川|South[ _-]*Korea|Seoul|Busan|Incheon|(?<![A-Za-z])(?:KR|KOR|ICN|PUS)(?![A-Za-z])|🇰🇷)", true}},
        {"UK", {R"(英国|英國|伦敦|倫敦|曼彻斯特|曼徹斯特|United[ _-]*Kingdom|Britain|London|Manchester|(?<![A-Za-z])(?:UK|GB|GBR|LHR|LGW|MAN)(?![A-Za-z])|🇬🇧)", true}},
        {"FR", {R"(法国|法國|巴黎|里昂|马赛|馬賽|France|Paris|Lyon|Marseille|(?<![A-Za-z])(?:FR|FRA|CDG|ORY|MRS)(?![A-Za-z])|🇫🇷)", true}},
        {"TR", {R"(土耳其|伊斯坦布尔|伊斯坦堡|安卡拉|Turkey|Turkiye|Türkiye|Istanbul|Ankara|(?<![A-Za-z])(?:TR|TUR|IST|SAW)(?![A-Za-z])|🇹🇷)", true}},
        {"AR", {R"(阿根廷|布宜诺斯艾利斯|布宜諾斯艾利斯|Argentina|Buenos[ _-]*Aires|(?<![A-Za-z])(?:AR|ARG|EZE|AEP)(?![A-Za-z])|🇦🇷)", true}},
        {"CA", {R"(加拿大|多伦多|多倫多|温哥华|溫哥華|蒙特利尔|蒙特婁|Canada|Toronto|Vancouver|Montreal|(?<![A-Za-z])(?:CA|CAN|YTO|YVR|YUL)(?![A-Za-z])|🇨🇦)", true}},
        {"AU", {R"(澳大利亚|澳洲|悉尼|墨尔本|布里斯班|Australia|Sydney|Melbourne|Brisbane|(?<![A-Za-z])(?:AU|AUS|SYD|MEL|BNE)(?![A-Za-z])|🇦🇺)", true}},
        {"NZ", {R"(新西兰|紐西蘭|奥克兰|奧克蘭|惠灵顿|威靈頓|New[ _-]*Zealand|Auckland|Wellington|(?<![A-Za-z])(?:NZ|NZL|AKL|WLG)(?![A-Za-z])|🇳🇿)", true}},
        {"DE", {R"(德国|德國|法兰克福|法蘭克福|柏林|慕尼黑|Germany|Frankfurt|Berlin|Munich|(?<![A-Za-z])(?:DE|DEU|FRA|BER|MUC)(?![A-Za-z])|🇩🇪)", true}},
        {"NL", {R"(荷兰|荷蘭|阿姆斯特丹|鹿特丹|Netherlands|Amsterdam|Rotterdam|(?<![A-Za-z])(?:NL|NLD|AMS|RTM)(?![A-Za-z])|🇳🇱)", true}},

        // Broad geographic continent & sub-region keyword presets (combined with child regions/countries)
        {"ASIA", {R"(亚洲|亞洲|亚太|亞太|Asia|APAC)", false}},
        {"EAST_ASIA", {R"(东亚|東亞|East[ _-]*Asia)", false}},
        {"SOUTHEAST_ASIA", {R"(东南亚|東南亞|Southeast[ _-]*Asia)", false}},
        {"SOUTH_ASIA", {R"(南亚|南亞|South[ _-]*Asia)", false}},
        {"MIDDLE_EAST", {R"(中东|中東|Middle[ _-]*East)", false}},
        {"EUROPE", {R"(欧洲|歐洲|欧盟|歐盟|Europe|(?-i:(?<![A-Za-z])EU(?![A-Za-z]))|🇪🇺)", false}},
        {"WESTERN_EUROPE", {R"(西欧|西歐|Western[ _-]*Europe)", false}},
        {"NORTHERN_EUROPE", {R"(北欧|北歐|Northern[ _-]*Europe)", false}},
        {"SOUTHERN_EUROPE", {R"(南欧|南歐|Southern[ _-]*Europe)", false}},
        {"EASTERN_EUROPE", {R"(东欧|東歐|Eastern[ _-]*Europe)", false}},
        {"AMERICA", {R"(美洲|拉美|拉丁美洲|America|LatAm)", false}},
        {"NORTH_AMERICA", {R"(北美|北美洲|North[ _-]*America)", false}},
        {"SOUTH_AMERICA", {R"(南美|南美洲|South[ _-]*America|Latin[ _-]*America)", false}},
        {"OCEANIA", {R"(大洋洲|Oceania)", false}},
        {"AFRICA", {R"(非洲|Africa)", false}},

        // Additional country presets (smallest unit = country; is_region=false so default OTHER still collects them)
        // Asia
        {"IN", {R"(印度|孟买|孟買|新德里|班加罗尔|班加羅爾|India|Mumbai|Delhi|Bangalore|(?-i:(?<![A-Za-z])(?:IN|IND|BOM|DEL|MAA)(?![A-Za-z]))|🇮🇳)", false}},
        {"MY", {R"(马来西亚|馬來西亞|大马|大馬|吉隆坡|Malaysia|Kuala[ _-]*Lumpur|(?-i:(?<![A-Za-z])(?:MY|MYS|KUL)(?![A-Za-z]))|🇲🇾)", false}},
        {"TH", {R"(泰国|泰國|曼谷|Thailand|Bangkok|(?-i:(?<![A-Za-z])(?:TH|THA|BKK)(?![A-Za-z]))|🇹🇭)", false}},
        {"VN", {R"(越南|胡志明|河内|河內|Vietnam|Hanoi|Ho[ _-]*Chi[ _-]*Minh|(?-i:(?<![A-Za-z])(?:VN|VNM|SGN|HAN)(?![A-Za-z]))|🇻🇳)", false}},
        {"PH", {R"(菲律宾|菲律賓|马尼拉|馬尼拉|Philippines|Manila|(?-i:(?<![A-Za-z])(?:PH|PHL|MNL)(?![A-Za-z]))|🇵🇭)", false}},
        {"ID", {R"(印度尼西亚|印尼|雅加达|雅加達|Indonesia|Jakarta|(?<![A-Za-z])(?:IDN|CGK)(?![A-Za-z])|🇮🇩)", false}},
        {"KH", {R"(柬埔寨|金边|金邊|Cambodia|Phnom[ _-]*Penh|(?-i:(?<![A-Za-z])(?:KH|KHM|PNH)(?![A-Za-z]))|🇰🇭)", false}},
        {"PK", {R"(巴基斯坦|卡拉奇|伊斯兰堡|Pakistan|Karachi|Islamabad|(?-i:(?<![A-Za-z])(?:PK|PAK|KHI)(?![A-Za-z]))|🇵🇰)", false}},
        {"BD", {R"(孟加拉|达卡|達卡|Bangladesh|Dhaka|(?-i:(?<![A-Za-z])(?:BD|BGD|DAC)(?![A-Za-z]))|🇧🇩)", false}},
        {"AE", {R"(阿联酋|阿聯酋|迪拜|杜拜|阿布扎比|United[ _-]*Arab[ _-]*Emirates|Dubai|Abu[ _-]*Dhabi|(?-i:(?<![A-Za-z])(?:AE|ARE|UAE|DXB|AUH)(?![A-Za-z]))|🇦🇪)", false}},
        {"IL", {R"(以色列|特拉维夫|特拉維夫|Israel|Tel[ _-]*Aviv|(?-i:(?<![A-Za-z])(?:IL|ISR|TLV)(?![A-Za-z]))|🇮🇱)", false}},
        {"SA", {R"(沙特|沙烏地|利雅得|Saudi|Riyadh|(?-i:(?<![A-Za-z])(?:SA|SAU|RUH)(?![A-Za-z]))|🇸🇦)", false}},
        // Europe
        {"CH", {R"(瑞士|苏黎世|蘇黎世|日内瓦|日內瓦|Switzerland|Zurich|Geneva|(?-i:(?<![A-Za-z])(?:CH|CHE|ZRH|GVA)(?![A-Za-z]))|🇨🇭)", false}},
        {"IE", {R"(爱尔兰|愛爾蘭|都柏林|Ireland|Dublin|(?-i:(?<![A-Za-z])(?:IE|IRL|DUB)(?![A-Za-z]))|🇮🇪)", false}},
        {"BE", {R"(比利时|比利時|布鲁塞尔|布魯塞爾|Belgium|Brussels|(?<![A-Za-z])(?:BEL|BRU)(?![A-Za-z])|🇧🇪)", false}},
        {"AT", {R"(奥地利|奧地利|维也纳|維也納|Austria|Vienna|(?<![A-Za-z])(?:AUT|VIE)(?![A-Za-z])|🇦🇹)", false}},
        {"LU", {R"(卢森堡|盧森堡|Luxembourg|(?-i:(?<![A-Za-z])(?:LU|LUX)(?![A-Za-z]))|🇱🇺)", false}},
        {"SE", {R"(瑞典|斯德哥尔摩|斯德哥爾摩|Sweden|Stockholm|(?-i:(?<![A-Za-z])(?:SE|SWE|ARN)(?![A-Za-z]))|🇸🇪)", false}},
        {"NO", {R"(挪威|奥斯陆|奧斯陸|Norway|Oslo|(?<![A-Za-z])(?:NOR|OSL)(?![A-Za-z])|🇳🇴)", false}},
        {"FI", {R"(芬兰|芬蘭|赫尔辛基|赫爾辛基|Finland|Helsinki|(?-i:(?<![A-Za-z])(?:FI|FIN|HEL)(?![A-Za-z]))|🇫🇮)", false}},
        {"DK", {R"(丹麦|丹麥|哥本哈根|Denmark|Copenhagen|(?-i:(?<![A-Za-z])(?:DK|DNK|CPH)(?![A-Za-z]))|🇩🇰)", false}},
        {"IS", {R"(冰岛|冰島|雷克雅未克|Iceland|Reykjavik|(?<![A-Za-z])(?:ISL|KEF)(?![A-Za-z])|🇮🇸)", false}},
        {"IT", {R"(意大利|義大利|米兰|米蘭|罗马|羅馬|Italy|Milan|Rome|(?<![A-Za-z])(?:ITA|MXP|FCO)(?![A-Za-z])|🇮🇹)", false}},
        {"ES", {R"(西班牙|马德里|馬德里|巴塞罗那|巴塞隆納|Spain|Madrid|Barcelona|(?-i:(?<![A-Za-z])(?:ES|ESP|MAD|BCN)(?![A-Za-z]))|🇪🇸)", false}},
        {"PT", {R"(葡萄牙|里斯本|Portugal|Lisbon|(?<![A-Za-z])(?:PRT|LIS)(?![A-Za-z])|🇵🇹)", false}},
        {"GR", {R"(希腊|希臘|雅典|Greece|Athens|(?-i:(?<![A-Za-z])(?:GR|GRC|ATH)(?![A-Za-z]))|🇬🇷)", false}},
        {"PL", {R"(波兰|波蘭|华沙|華沙|Poland|Warsaw|(?-i:(?<![A-Za-z])(?:PL|POL|WAW)(?![A-Za-z]))|🇵🇱)", false}},
        {"CZ", {R"(捷克|布拉格|Czech|Prague|(?-i:(?<![A-Za-z])(?:CZ|CZE|PRG)(?![A-Za-z]))|🇨🇿)", false}},
        {"HU", {R"(匈牙利|布达佩斯|布達佩斯|Hungary|Budapest|(?-i:(?<![A-Za-z])(?:HU|HUN|BUD)(?![A-Za-z]))|🇭🇺)", false}},
        {"RO", {R"(罗马尼亚|羅馬尼亞|布加勒斯特|Romania|Bucharest|(?-i:(?<![A-Za-z])(?:RO|ROU|OTP)(?![A-Za-z]))|🇷🇴)", false}},
        {"BG", {R"(保加利亚|保加利亞|索非亚|索菲亞|Bulgaria|Sofia|(?-i:(?<![A-Za-z])(?:BG|BGR|SOF)(?![A-Za-z]))|🇧🇬)", false}},
        {"UA", {R"(乌克兰|烏克蘭|基辅|基輔|Ukraine|Kyiv|Kiev|(?<![A-Za-z])(?:UKR|KBP)(?![A-Za-z])|🇺🇦)", false}},
        // America
        {"MX", {R"(墨西哥|Mexico|(?-i:(?<![A-Za-z])(?:MX|MEX)(?![A-Za-z]))|🇲🇽)", false}},
        {"BR", {R"(巴西|圣保罗|聖保羅|Brazil|Sao[ _-]*Paulo|São[ _-]*Paulo|(?-i:(?<![A-Za-z])(?:BR|BRA|GRU)(?![A-Za-z]))|🇧🇷)", false}},
        {"CL", {R"(智利|圣地亚哥|聖地牙哥|Chile|Santiago|(?-i:(?<![A-Za-z])(?:CL|CHL|SCL)(?![A-Za-z]))|🇨🇱)", false}},
        {"CO", {R"(哥伦比亚|哥倫比亞|波哥大|Colombia|Bogota|(?<![A-Za-z])(?:COL|BOG)(?![A-Za-z])|🇨🇴)", false}},
        {"PE", {R"(秘鲁|秘魯|利马|利馬|Peru|Lima|(?<![A-Za-z])(?:PER|LIM)(?![A-Za-z])|🇵🇪)", false}},
        {"UY", {R"(乌拉圭|烏拉圭|蒙得维的亚|Uruguay|Montevideo|(?-i:(?<![A-Za-z])(?:UY|URY|MVD)(?![A-Za-z]))|🇺🇾)", false}},
        // Africa
        {"ZA", {R"(南非|约翰内斯堡|約翰尼斯堡|开普敦|開普敦|South[ _-]*Africa|Johannesburg|Cape[ _-]*Town|(?-i:(?<![A-Za-z])(?:ZA|ZAF|JNB|CPT)(?![A-Za-z]))|🇿🇦)", false}},
        {"EG", {R"(埃及|开罗|開羅|Egypt|Cairo|(?<![A-Za-z])(?:EGY|CAI)(?![A-Za-z])|🇪🇬)", false}},
        {"NG", {R"(尼日利亚|奈及利亞|拉各斯|Nigeria|Lagos|(?<![A-Za-z])(?:NGA|LOS)(?![A-Za-z])|🇳🇬)", false}},
        {"KE", {R"(肯尼亚|肯亞|内罗毕|奈洛比|Kenya|Nairobi|(?<![A-Za-z])(?:KEN|NBO)(?![A-Za-z])|🇰🇪)", false}},

        // Restricted/sanctioned countries (used by official AI exclusions & continent macros)
        {"CN", {R"(中国|中國|大陆|大陸|回国|回國|上海|北京|广州|廣州|深圳|China|Mainland|Shanghai|Beijing|Guangzhou|Shenzhen|(?<![A-Za-z])(?:CN(?![A-Za-z]|2(?![0-9]))|CHN|PRC|BJS|PEK|PVG|SZX)(?![A-Za-z])|🇨🇳)", false}},
        {"MO", {R"(澳门|澳門|Macau|Macao|(?<![A-Za-z])(?:MO|MFM)(?![A-Za-z])|🇲🇴)", false}},
        {"RU", {R"(俄罗斯|俄羅斯|俄国|俄國|莫斯科|圣彼得堡|聖彼得堡|Russia|Moscow|St\.?[ _-]*Petersburg|(?<![A-Za-z])(?:RU|RUS|MOW|SVO)(?![A-Za-z])|🇷🇺)", false}},
        {"BY", {R"(白俄罗斯|白俄羅斯|白俄|明斯克|Belarus|Minsk|(?<![A-Za-z])(?:BLR|MSQ)(?![A-Za-z])|(?-i:(?<![A-Za-z])BY(?![A-Za-z]))|🇧🇾)", false}},
        {"IR", {R"(伊朗|德黑兰|德黑蘭|Iran|Tehran|(?<![A-Za-z])(?:IR|IRN|IKA)(?![A-Za-z])|🇮🇷)", false}},
        {"KP", {R"(朝鲜|朝鮮|北韩|北韓|平壤|North[ _-]*Korea|Pyongyang|(?<![A-Za-z])(?:KP|PRK|DPRK)(?![A-Za-z])|🇰🇵)", false}},
        {"SY", {R"(叙利亚|敘利亞|大马士革|Syria|Damascus|(?<![A-Za-z])(?:SY|SYR)(?![A-Za-z])|🇸🇾)", false}},
        {"CU", {R"(古巴|哈瓦那|Cuba|Havana|(?<![A-Za-z])(?:CUB|HAV)(?![A-Za-z])|🇨🇺)", false}},
        {"VE", {R"(委内瑞拉|委內瑞拉|加拉加斯|Venezuela|Caracas|(?<![A-Za-z])(?:VE|VEN|CCS)(?![A-Za-z])|🇻🇪)", false}},
        {"AF", {R"(阿富汗|喀布尔|Afghanistan|Kabul|(?<![A-Za-z])(?:AF|AFG|KBL)(?![A-Za-z])|🇦🇫)", false}},
        {"MM", {R"(缅甸|緬甸|仰光|Myanmar|Burma|Yangon|(?<![A-Za-z])(?:MM|MMR|RGN)(?![A-Za-z])|🇲🇲)", false}},
        {"SD", {R"(苏丹|蘇丹|喀土穆|Sudan|Khartoum|(?<![A-Za-z])(?:SDN|KRT)(?![A-Za-z])|🇸🇩)", false}},
        {"SO", {R"(索马里|索馬里|摩加迪沙|Somalia|Mogadishu|(?<![A-Za-z])(?:SOM|MGQ)(?![A-Za-z])|🇸🇴)", false}},
        {"YE", {R"(也门|葉門|萨那|Yemen|Sanaa|(?<![A-Za-z])(?:YE|YEM|SAH)(?![A-Za-z])|🇾🇪)", false}},
        {"ZW", {R"(津巴布韦|辛巴威|哈拉雷|Zimbabwe|Harare|(?<![A-Za-z])(?:ZW|ZWE|HRE)(?![A-Za-z])|🇿🇼)", false}},

        // Dedicated AI & scenario node keyword presets (also combined automatically by AI macros)
        {"AI", {R"(人工智能|(?<![a-z])ai(?![a-z]))", false}},
        {"CHATGPT", {R"(chat[ _-]*gpt|open[ _-]*ai|openai|(?<![a-z])gpt(?:[ _-]*[0-9a-z.]+)?|copilot)", false}},
        {"CODEX", {R"(codex|open[ _-]*ai|openai|chat[ _-]*gpt|(?<![a-z])gpt(?:[ _-]*[0-9a-z.]+)?|copilot)", false}},
        {"CLAUDE", {R"(claude|anthropic)", false}},
        {"GEMINI", {R"(gemini|google[ _-]*ai|(?<![a-z])bard(?![a-z])|aistudio|antigravity)", false}},
        {"GROK", {R"(grok|(?<![a-z])x[ _-]*ai(?![a-z]))", false}},
        {"NETFLIX", {R"(netflix|(?<![a-z])nf(?![a-z])|奈飞|奈菲)", false}},
        {"MEDIA", {R"(流媒体|媒体|解锁|media|netflix|disney|youtube|spotify|tiktok|hbo|emby)", false}},
        {"GAME", {R"(游戏|遊戲|(?<![a-z])game(?![a-z])|低延迟|专线|iepl|iplc)", false}},
        {"RESIDENTIAL", {R"(家宽|家寬|住宅|原生|双isp|雙isp|(?<![a-z])(?:isp|residential|native)(?![a-z]))", false}}
    };
    return presets;
}

inline std::string stripCaseInsensitivePrefix(const std::string &pattern)
{
    std::string trimmed = trim(pattern);
    if(startsWith(trimmed, "(?i)"))
        return trimmed.substr(4);
    return trimmed;
}

inline std::string combineRegionPatterns(const std::string &base, const std::string &extra)
{
    std::string b = stripCaseInsensitivePrefix(base);
    std::string e = stripCaseInsensitivePrefix(extra);
    if(b.empty())
        return e;
    if(e.empty())
        return b;
    return "(?:" + b + ")|(?:" + e + ")";
}

inline void applyRegionRules(RegionPresetMap &presets, const RegionRuleConfigs &rules)
{
    for(const RegionRule &rule : rules)
    {
        std::string key = normalizeRegionName(rule.name);
        std::string pat = trim(rule.pattern);
        if(key.empty() || key == "OTHER" || key == "ALL" || pat.empty())
            continue;

        auto iter = presets.find(key);
        if(rule.append)
        {
            if(iter != presets.end())
                iter->second.pattern = combineRegionPatterns(iter->second.pattern, pat);
            else
                presets.emplace(key, RegionPreset{pat, rule.is_region});
        }
        else
        {
            bool is_reg = (iter != presets.end()) ? iter->second.is_region : rule.is_region;
            presets[key] = RegionPreset{pat, is_reg};
        }
    }
}

inline RegionRuleConfigs parseRegionRulesFromIni(const string_array &items, bool is_append)
{
    RegionRuleConfigs rules;
    for(const std::string &item : items)
    {
        std::string::size_type pos = item.find(',');
        if(pos == std::string::npos)
            continue;
        std::string name = trim(item.substr(0, pos));
        std::string pattern = trim(item.substr(pos + 1));
        if(name.empty() || pattern.empty())
            continue;
        rules.push_back(RegionRule{std::move(name), std::move(pattern), is_append, true});
    }
    return rules;
}

inline string_array splitRegionSpecs(const std::string &spec_list)
{
    string_array result;
    std::string current;
    int paren_depth = 0, bracket_depth = 0, brace_depth = 0;
    bool escaped = false;

    for(char c : spec_list)
    {
        if(escaped)
        {
            current += c;
            escaped = false;
            continue;
        }
        if(c == '\\')
        {
            current += c;
            escaped = true;
            continue;
        }
        if(c == '(' && bracket_depth == 0)
            paren_depth++;
        else if(c == ')' && bracket_depth == 0 && paren_depth > 0)
            paren_depth--;
        else if(c == '[')
            bracket_depth++;
        else if(c == ']' && bracket_depth > 0)
            bracket_depth--;
        else if(c == '{' && bracket_depth == 0)
            brace_depth++;
        else if(c == '}' && bracket_depth == 0 && brace_depth > 0)
            brace_depth--;

        if(c == ',' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0)
        {
            std::string trimmed = trim(current);
            if(!trimmed.empty())
                result.emplace_back(std::move(trimmed));
            current.clear();
        }
        else
        {
            current += c;
        }
    }
    std::string trimmed = trim(current);
    if(!trimmed.empty())
        result.emplace_back(std::move(trimmed));
    return result;
}

inline string_array getRegionMacroChildren(const std::string &key, const RegionPresetMap &presets)
{
    static const string_array canonical_order = {
        "HK", "TW", "JP", "SG", "KR", "US", "CA", "AU", "NZ", "UK", "DE", "NL", "FR", "TR", "AR"
    };
    if(key == "ALL")
    {
        string_array out;
        std::set<std::string> added;
        for(const std::string &k : canonical_order)
        {
            auto iter = presets.find(k);
            if(iter != presets.end() && iter->second.is_region && !iter->second.pattern.empty())
            {
                out.emplace_back(k);
                added.emplace(k);
            }
        }
        for(const auto &entry : presets)
        {
            if(entry.second.is_region && !entry.second.pattern.empty() && !added.count(entry.first))
                out.emplace_back(entry.first);
        }
        return out;
    }

    // Broad -> narrow geographic hierarchy (continent -> sub-region -> country)
    if(key == "ASIA")
        return {"EAST_ASIA", "SOUTHEAST_ASIA", "SOUTH_ASIA", "MIDDLE_EAST"};
    if(key == "EAST_ASIA")
        return {"HK", "TW", "JP", "KR", "CN", "MO"};
    if(key == "SOUTHEAST_ASIA")
        return {"SG", "MY", "TH", "VN", "PH", "ID", "KH", "MM"};
    if(key == "SOUTH_ASIA")
        return {"IN", "PK", "BD"};
    if(key == "MIDDLE_EAST")
        return {"TR", "AE", "IL", "SA", "IR", "SY", "YE", "AF"};
    if(key == "EUROPE")
        return {"WESTERN_EUROPE", "NORTHERN_EUROPE", "SOUTHERN_EUROPE", "EASTERN_EUROPE"};
    if(key == "WESTERN_EUROPE")
        return {"UK", "DE", "NL", "FR", "CH", "IE", "BE", "AT", "LU"};
    if(key == "NORTHERN_EUROPE")
        return {"SE", "NO", "FI", "DK", "IS"};
    if(key == "SOUTHERN_EUROPE")
        return {"IT", "ES", "PT", "GR"};
    if(key == "EASTERN_EUROPE")
        return {"PL", "CZ", "HU", "RO", "BG", "UA", "RU", "BY"};
    if(key == "AMERICA")
        return {"NORTH_AMERICA", "SOUTH_AMERICA"};
    if(key == "NORTH_AMERICA")
        return {"US", "CA", "MX", "CU"};
    if(key == "SOUTH_AMERICA")
        return {"AR", "BR", "CL", "CO", "PE", "UY", "VE"};
    if(key == "OCEANIA")
        return {"AU", "NZ"};
    if(key == "AFRICA")
        return {"ZA", "EG", "NG", "KE", "SD", "SO", "ZW"};

    // Single-name AI macros: dedicated service/AI nodes first, then preferred regions,
    // then all remaining nodes outside officially unsupported/sanctioned jurisdictions.
    if(key == "CHATGPT" || key == "CODEX")
    {
        return {"AI", "TW", "JP", "SG", "KR", "US", "CA", "AU", "NZ", "UK", "DE", "NL", "FR", "TR", "AR",
                "OTHER(CN,HK,MO,RU,BY,IR,KP,SY,CU,VE,AF)"};
    }
    if(key == "CLAUDE")
    {
        return {"AI", "TW", "JP", "SG", "KR", "US", "CA", "AU", "NZ", "UK", "DE", "NL", "FR", "TR", "AR",
                "OTHER(CN,HK,MO,RU,BY,IR,KP,SY,CU,VE,AF,MM,SD,SO,YE,ZW)"};
    }
    if(key == "GEMINI")
    {
        return {"AI", "TW", "JP", "SG", "KR", "US", "CA", "AU", "NZ", "UK", "DE", "NL", "FR", "TR", "AR",
                "OTHER(CN,HK,MO,RU,BY,IR,KP,SY,CU)"};
    }
    if(key == "GROK")
    {
        return {"AI", "US", "TW", "JP", "SG", "KR", "CA", "AU", "NZ", "UK", "DE", "NL", "FR", "TR", "AR",
                "OTHER(CN,HK,MO,RU,BY,IR,KP,SY,CU,VE)"};
    }
    return {};
}

inline void collectMacroKeysRecursive(const std::string &key, const RegionPresetMap &presets, string_array &out, std::set<std::string> &visited, bool include_other)
{
    if(!visited.emplace(key).second)
        return;

    // Include key's own direct pattern if defined (e.g. continent keyword or dedicated AI keyword)
    auto iter = presets.find(key);
    if(iter != presets.end() && !iter->second.pattern.empty())
        out.emplace_back(key);

    string_array children = getRegionMacroChildren(key, presets);
    for(const std::string &child : children)
    {
        if(startsWith(toUpper(child), "OTHER("))
        {
            if(include_other)
                out.emplace_back(child);
            continue;
        }
        collectMacroKeysRecursive(normalizeRegionName(child), presets, out, visited, include_other);
    }
}

inline string_array expandRegionMacroKeys(const std::string &key, const RegionPresetMap &presets, bool include_other = true)
{
    string_array children = getRegionMacroChildren(key, presets);
    if(children.empty())
        return {};
    string_array out;
    std::set<std::string> visited;
    collectMacroKeysRecursive(key, presets, out, visited, include_other);
    return out;
}

inline bool isBroadRegionMacro(const std::string &key)
{
    static const std::set<std::string> broad = {
        "ASIA", "EUROPE", "AMERICA", "OCEANIA", "AFRICA",
        "EAST_ASIA", "SOUTHEAST_ASIA", "SOUTH_ASIA", "MIDDLE_EAST",
        "WESTERN_EUROPE", "NORTHERN_EUROPE", "SOUTHERN_EUROPE", "EASTERN_EUROPE",
        "NORTH_AMERICA", "SOUTH_AMERICA"
    };
    return broad.count(key) > 0;
}

inline bool isRegionMacroChildOf(const std::string &child_key, const std::string &parent_key, const RegionPresetMap &presets)
{
    if(child_key == parent_key || !isBroadRegionMacro(parent_key))
        return false;
    string_array expanded = expandRegionMacroKeys(parent_key, presets, false);
    return std::find(expanded.begin(), expanded.end(), child_key) != expanded.end();
}

inline std::string buildOtherRegex(const string_array &excluded_keys, const RegionPresetMap &presets, const std::string &inline_extra)
{
    std::string combined;
    auto append_key = [&](const std::string &k)
    {
        auto iter = presets.find(k);
        if(iter == presets.end() || iter->second.pattern.empty())
            return;
        if(!combined.empty())
            combined += "|";
        combined += "(?:" + stripCaseInsensitivePrefix(iter->second.pattern) + ")";
    };

    if(excluded_keys.empty())
    {
        for(const auto &entry : presets)
        {
            if(entry.second.is_region && !entry.second.pattern.empty())
                append_key(entry.first);
        }
    }
    else
    {
        std::set<std::string> seen;
        for(const std::string &raw_k : excluded_keys)
        {
            std::string k = normalizeRegionName(raw_k);
            string_array macro = expandRegionMacroKeys(k, presets, false);
            if(!macro.empty())
            {
                for(const std::string &mk : macro)
                {
                    if(seen.emplace(mk).second)
                        append_key(mk);
                }
            }
            else if(seen.emplace(k).second)
            {
                append_key(k);
            }
        }
    }

    std::string other_regex = combined.empty() ? ".*" : ("(?i)^(?!.*(?:" + combined + ")).*$");
    if(!inline_extra.empty())
        return "(?:" + other_regex + ")|(?:(?i)(?:" + stripCaseInsensitivePrefix(inline_extra) + "))";
    return other_regex;
}

inline std::string buildRegionRegex(const std::string &spec, const RegionPresetMap &presets, const std::set<std::string> *extra_excludes = nullptr)
{
    std::string name_part = spec, inline_extra;
    std::string::size_type plus_pos = spec.find('+');
    if(plus_pos != std::string::npos)
    {
        name_part = trim(spec.substr(0, plus_pos));
        inline_extra = trim(spec.substr(plus_pos + 1));
    }
    else
    {
        name_part = trim(name_part);
    }

    // Support OTHER(HK,TW,JP,SG,US) for custom exclusion subset
    if(startsWith(toUpper(name_part), "OTHER(") && endsWith(name_part, ")"))
    {
        std::string inner = name_part.substr(6, name_part.size() - 7);
        string_array subset = splitRegionSpecs(inner);
        if(extra_excludes)
        {
            for(const std::string &ex : *extra_excludes)
                subset.emplace_back(ex);
        }
        return buildOtherRegex(subset, presets, inline_extra);
    }

    std::string key = normalizeRegionName(name_part);
    if(key.empty())
        return "";

    if(key == "OTHER")
    {
        if(extra_excludes && !extra_excludes->empty())
        {
            string_array subset;
            for(const auto &entry : presets)
            {
                if(entry.second.is_region && !entry.second.pattern.empty())
                    subset.emplace_back(entry.first);
            }
            for(const std::string &ex : *extra_excludes)
                subset.emplace_back(ex);
            return buildOtherRegex(subset, presets, inline_extra);
        }
        return buildOtherRegex({}, presets, inline_extra);
    }

    auto iter = presets.find(key);
    if(iter != presets.end() && !iter->second.pattern.empty())
    {
        std::string body = inline_extra.empty()
            ? stripCaseInsensitivePrefix(iter->second.pattern)
            : combineRegionPatterns(iter->second.pattern, inline_extra);
        return "(?i)(?:" + body + ")";
    }

    if(!inline_extra.empty())
        return "(?i)(?:" + stripCaseInsensitivePrefix(inline_extra) + ")";

    return "";
}

inline string_array expandRegionSpecToRegexes(const std::string &spec_list, const RegionPresetMap &presets)
{
    string_array tokens = splitRegionSpecs(spec_list);
    std::set<std::string> excluded;
    for(const std::string &tok : tokens)
    {
        if(!tok.empty() && tok[0] == '!')
        {
            std::string key = normalizeRegionName(tok.substr(1));
            string_array macro = expandRegionMacroKeys(key, presets, false);
            if(!macro.empty())
            {
                for(const std::string &mk : macro)
                    excluded.emplace(mk);
            }
            else if(!key.empty())
            {
                excluded.emplace(key);
            }
        }
    }

    string_array regexes;
    auto push_unique = [&regexes](std::string reg)
    {
        if(!reg.empty() && std::find(regexes.begin(), regexes.end(), reg) == regexes.end())
            regexes.emplace_back(std::move(reg));
    };

    for(const std::string &tok : tokens)
    {
        if(tok.empty() || tok[0] == '!')
            continue;

        std::string base_name = tok;
        std::string::size_type plus_pos = tok.find('+');
        if(plus_pos != std::string::npos)
            base_name = tok.substr(0, plus_pos);

        std::string key = normalizeRegionName(base_name);
        string_array macro = (plus_pos == std::string::npos) ? expandRegionMacroKeys(key, presets, true) : string_array{};
        if(!macro.empty())
        {
            for(const std::string &mk : macro)
            {
                if(excluded.count(mk))
                    continue;
                push_unique(buildRegionRegex(mk, presets, &excluded));
            }
            continue;
        }

        if(excluded.count(key))
            continue;
        push_unique(buildRegionRegex(tok, presets, &excluded));
    }
    return regexes;
}

#endif // REGION_H_INCLUDED
