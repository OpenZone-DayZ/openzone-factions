// Серверний модуль фракційної системи.
//
// Усе, що ядро колись робило для фракцій, робиться тепер тут: читання
// конфігу, підписки на роди моста, реєстрація адмінської сторінки, RPC змін
// ролей і прибирання за гравцем на виході.
//
// ЯДРО ПРО ЦЕЙ МОД НЕ ЗНАЄ. Зв'язок односторонній: ми підставляємо свою
// реалізацію в OZ_Identity, і всі, кому треба знати, чия людина перед ними,
// питають ЯДРО, а не нас. Тому КПК, рація й будь-який майбутній мод
// компілюються й працюють без цього pbo.
//
// РОДИ МОСТА ПІДПИСУЄМО САМІ. Раніше це робило ядро -- за нас і за КПК, --
// і через це список родів у ядрі мусив знати про кожен мод серії. Тепер
// кожен просить своє, і ПІДПИСКА Й Є ЄДИНЕ ДЖЕРЕЛО ПРАВДИ: рід їздить мостом
// тому, що хтось його попросив, і більш ні через що.
//
// Тут стояло «а сервер вирішує, що дозволити (OZ_BridgeSettings.Kinds)». Того
// рубильника немає з ТЗ-5 R-C1.4: він був другим списком з майже тим самим
// іменем, що й Mirrors, і саме такою парою можна було непомітно вимкнути
// пермадес -- рід "wipe" не возиться, а адмін бачить дзеркала ввімкненими.
// Mirrors лишився й вирішує ІНШЕ питання -- що з возимого показувати в
// гільдії.

[CF_RegisterModule(OZF_Module)]
class OZF_Module : CF_ModuleWorld
{
    override void OnInit()
    {
        super.OnInit();

        EnableMissionStart();
        // ClientDisconnect, а не InvokeDisconnect: див. OnClientDisconnect.
        EnableClientDisconnect();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        if (!GetGame().IsServer())
            return;

        // ВЛАСНІ НАЛАШТУВАННЯ, а не розділ у конфігу ядра (2026-09-04):
        // OZ_Factions_Settings.json зі своїм InviteTtlSeconds, дефолт 120.
        OZF_Settings.ServerLoad();

        OZ_Factions.ServerLoad();
        OZF_Loadouts.ServerLoad();

        // Спорядження на появі (ТЗ-3): драбина фракцій, звань, посад і міток
        // живе тут; ядро лише роздягає й одягає те, що ми назвемо.
        OZ_Loadout.Provide(new OZF_LoadoutService());

        // Ось той самий односторонній зв'язок: ядро отримує реалізацію й
        // роздає її всім охочим, не знаючи, звідки вона.
        OZ_Identity.Provide(new OZF_Identity());

        // Адмінський РОЗДІЛ -- НАШ, окремо від ядрових. Ядро лишає собі
        // спавни й редактор конфігів; ролі, ранги, звання й призначення
        // фракцій живуть тут, разом із кодом, який їх виконує.
        //
        // РОЗДІЛ, А НЕ СТОРІНКА (ТЗ-5 §C2). Саме через це ця половина вкладки
        // VPP не працювала жодного разу: сторінку "factions" не просив ніхто
        // в усьому дереві -- клієнт слав усе на "admin", де знали лише
        // cfg_*, -- а якби й просив, гейт КПК вимагав би від адміна
        // розімкнений увімкнений прилад із цією сторінкою в профілі.
        OZ_AdminRegistry.Register(OZF_Const.SECTION, new OZF_AdminSection());

        // Ролі й ростер -- НАШІ роди. Підписка з OnMissionStart: міст
        // стартує на тік пізніше саме для того, щоб ця встигла.
        OZ_BridgeClient.Subscribe("roles", new OZ_RolesSink());
        OZ_BridgeClient.Subscribe("roster", new OZ_RosterSink());

        // ПЕРМАДЕС: стираємо СВОЄ, і більше нічиє.
        //
        // Тут стояла підписка на рід "wipe" -- і саме через неї сервер без
        // цього мода не чув команди бота взагалі: гра не робила своєї
        // половини, ролі в Discord скидались, а КПК небіжчика працювали далі.
        // Підписка переїхала в ядро разом зі службою (рішення власника
        // 2026-09-08), а нам лишилось назвати те, що пишемо самі.
        //
        // Моста витирач не кличе: передача основ угруповань і скидання ролей
        // до новачка -- половина бота, і вона вже сталась.
        OZ_Wipe.Register("factions", new OZF_Wiper());

        // КАНАЛ РОЛЕЙ -- НАШ, під нашим іменем мода (2026-09-04). Був ядровим
        // (OZ_Rpc.RegisterRoles), тобто ядро тримало реєстрацію заради гри,
        // якої в ньому немає. Зворотного каналу немає зовсім: відповіді їдуть
        // ядровим "OZ_Notice", який усе одно реєструється кожним клієнтом.
        GetRPCManager().AddRPC(OZF_Const.MOD, OZF_Rpc.RPC_ROLE_REQ, this, SingleplayerExecutionType.Server);

        // ЧИСЛО У ФОРМІ `ключ=значення`, як у ядровому «core loaded: ...».
        // Це не косметика: вердикт стенду читає лічильники САМЕ з рядка
        // готовності і саме в цій формі, тож «9 faction(s)» був числом для
        // людини й порожнечею для перевірки (зміряно 2026-09-04: оголошений
        // у профілі `factions = 9` не міг збігтися ніколи).
        OZ_Log.Info("factions loaded: factions=" + OZ_Factions.Count().ToString());

        // МІСТ ДЛЯ ФРАКЦІЙ ОБОВ'ЯЗКОВИЙ, і мовчати про це не можна (ТЗ-2 R2.1).
        //
        // Базову фракцію призначає гра й тримає у файлі гравця, тож вона є й
        // при мертвому мості (ТЗ-1 R5.4). Усе інше -- угруповання, звання,
        // трейти, склад, лідерство і зняття ролей на виході -- живе в боті, і
        // без нього цей мод не працює, а ЗОБРАЖУЄ роботу: екрани малюються,
        // склад порожній, лідерські кнопки відмовляють по одній.
        //
        // Конфігурації «фракції без моста» не існує. Кажемо це рядком на
        // буті, а не залишаємо адмінові з'ясовувати по симптомах.
        OZ_BridgeSettings b = OZ_Settings.Get().Bridge;
        if (!b || !b.Enabled)
            OZ_Log.Warn("factions need the bridge: the bot owns organisations, ranks, traits and leadership. With Bridge.Enabled false only the base faction works");
    }

    // Гравець вийшов: прибираємо те, що має сенс лише поки він тут.
    //
    // ПРОЕКЦІЮ РОЛЕЙ (OZ_Roles.Forget) ТУТ НЕ ЧІПАЄМО, хоч колись задумано
    // було саме це. Міст шле проекцію лише ПРИ ЗМІНІ й пам'ятає, що цьому
    // серверу вже віддав; забуває -- і перешле -- лише тоді, коли uid випаде
    // з опиту (openzone-bridge/src/index.js, rolesSeen, «Forget whoever
    // left»). Вихід гравця з опиту його не виводить: КПК, який хтось носить,
    // додає до опиту сесійний uid хазяїна (OZ_PdaUidProvider), та й
    // перезахід, що вміщається між двома опитами, випасти не встигає. Стерта
    // тут проекція не повернулась би до першої зміни ролей у Discord, і гра
    // вважала б людину одинаком -- на чужому КПК, а після перезаходу й на
    // ній самій. Кеш проекцій живе весь запуск (див. OZ_Roles); запис
    // відсутнього перекриє свіжа проекція, щойно міст її перешле.
    //
    // ПОДІЯ -- OnClientDisconnect. Тут стояв OnInvokeDisconnect, і за весь час
    // він не прибрав нічого: CF кличе його з базовими аргументами, Cast до
    // CF_EventPlayerDisconnectedArgs давав null. А UID у справжніх аргументах
    // виходу -- хеш, не Steam64, під яким лежать наші мапи; Steam64 називає
    // ядро (OZ_Players.PlainOfLeaving).
    override void OnClientDisconnect(Class sender, CF_EventArgs args)
    {
        super.OnClientDisconnect(sender, args);

        if (!GetGame().IsServer())
            return;

        string uid = OZ_Players.PlainOfLeaving(args);
        if (uid == "")
            return;

        // Запрошення до того, хто вийшов, показувати більше нікому. Стояло в
        // ядрі й пережило винесення -- через що набір без цього мода не
        // компілювався зовсім; місце йому тут, поруч із рештою нашого.
        OZ_FactionInvites.Forget(uid);

        // І його місце в лічильнику запитів до моста.
        OZ_RoleOps.ForgetActor(uid);
    }


    // Зміна ролей із гри. Особа -- ЗАВЖДИ з sender: клієнт не називає, від
    // чийого імені просить, і не може -- у конверті немає такого поля.
    //
    // Ім'я методу -- рядок OZF_Rpc.RPC_ROLE_REQ посимвольно.
    void OZF_RoleReq(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type != CallType.Server)
            return;

        Param3<string, string, string> data;
        if (!ctx.Read(data))
            return;

        if (!sender)
            return;

        string op         = data.param1;
        string targetName = data.param2;
        string arg        = data.param3;

        // ДВІ ФОРМИ АДРЕСИ, і жодної третьої. Клієнт назвав рядок; кому він
        // належить, вирішуємо ми.
        //
        // АДМІНСЬКИЙ ВИНЯТОК: адреса "uid:<steam64>" називає особу точно.
        // Приймається ЛИШЕ від адміна -- консоль і так бачить uid-и в
        // ростері; межа «клієнт не оперує чужими Steam64» стоїть для гравців,
        // не для адмінки.
        string targetUid = "";
        if (targetName.IndexOf("key:") == 0)
        {
            // КЛЮЧ ПЕРСОНАЖА (ТЗ-4 R-C4.1) у тій самій непрозорій формі, що
            // й у контактах: клієнт бачить хеш, не Steam64. Розгортається
            // лише серед тих, кого відправник і так може назвати -- його
            // друзі, його угруповання, присутні, -- і називає ОДНЕ живе
            // життя: ключ вайпнутого персонажа не називає нікого.
            string tag = targetName.Substring(4, targetName.Length() - 4);
            targetUid = OZ_RoleOps.UidByTag(tag, sender.GetPlainId());
            if (targetUid == "")
            {
                OZF_Rpc.RoleRespond(sender, op, false, "STR_OZ_ERR_NO_TARGET");
                return;
            }
        }
        else if (targetName.IndexOf("uid:") == 0)
        {
            if (!OZ_Perm.IsAdmin(sender))
            {
                OZF_Rpc.RoleRespond(sender, op, false, "STR_OZ_ERR_ADMIN_ONLY");
                return;
            }
            targetUid = targetName.Substring(4, targetName.Length() - 4);
        }
        else if (targetName != "")
        {
            // ТРЕТЬОЇ ФОРМИ АДРЕСИ НЕМАЄ (2026-09-06). Тут приймалось голе
            // ігрове ім'я -- шлях із часів до ключа персонажа, який мовчки не
            // працював ні на відсутніх, ні на тезках. Жоден екран серії ним не
            // користується; усе, що не "key:" і не "uid:", -- це або старий
            // клієнт, або підроблений конверт, і відповідь на обидва однакова.
            OZF_Rpc.RoleRespond(sender, op, false, "STR_OZ_ERR_NO_TARGET");
            return;
        }

        // Запрошення -- НЕ операція над ролями, тому й не йде в OZ_RoleOps:
        // до згоди воно взагалі нічого не міняє в Discord.
        if (op == "invite")
        {
            OZ_FactionInvites.Offer(sender, targetUid);
            return;
        }

        if (op == "accept")
        {
            OZ_FactionInvites.Accept(sender);
            return;
        }

        if (op == "decline")
        {
            OZ_FactionInvites.Decline(sender);
            return;
        }

        // ПІТИ САМОМУ. Ціль -- завжди сам відправник, і саме тому це окрема
        // операція, а не faction.clear з власним ім'ям у полі: ім'я треба
        // спершу знайти серед присутніх, а піти з фракції людина має право
        // незалежно від того, чи є в Зоні хтось із таким самим ім'ям.
        if (op == "leave")
        {
            OZ_RoleOps.Request(sender, sender.GetPlainId(), OZ_RoleOp.FACTION_CLEAR, "");
            return;
        }

        // ЗОН СПАВНА ТУТ БІЛЬШЕ НЕМАЄ. Вони повернулись у ядро власним
        // адмінським розділом (ТЗ-5 §C1 R6, §C4 R-C4.6): зони, файл зон і
        // панель SPAWNS у вкладці VPP -- ядрові, і поки їхній обробник жив
        // тут, сервер без мода фракцій не міг завести жодної зони.
        OZ_RoleOps.Request(sender, targetUid, op, arg);
    }
}

// Реалізація служби ядра. Тонка обгортка й нічого більше: правила живуть у
// OZ_Factions та OZ_Roles, а тут лише переклад із мови ядра на нашу.
class OZF_Identity : OZ_IdentityService
{
    override string BaseOf(string uid)
    {
        return OZ_Factions.BaseOfUid(uid);
    }

    override string OrgOf(string uid)
    {
        return OZ_Factions.OrgOfUid(uid);
    }

    override string FactionName(string id)
    {
        return OZ_Factions.NameOf(id);
    }

    override string OrgOfPlayer(PlayerBase player, string uid)
    {
        return OZ_Factions.OrgOf(player, uid);
    }

    // Перший вхід: базова фракція з'являється тут і більше ніде.
    //
    // «Яка саме» -- перша з BaseFaction: true в порядку OZ_Factions.json
    // (ТЗ-1 R5.2). Порядок файлу і є відповіддю: перевпорядкувати його адмін
    // уміє, а окреме поле «головна базова» було б другим джерелом правди про
    // одне й те саме.
    override void EnsureBase(string uid)
    {
        if (!GetGame().IsServer())
            return;
        if (uid == "")
            return;
        if (OZ_Factions.BaseOfUid(uid) != "")
            return;

        string slug = OZ_Factions.FirstBaseId();
        if (slug == "")
        {
            // РАЗ НА ЗАПУСК, а не на кожен вхід. Це стан файла налаштувань:
            // він не зміниться від того, що зайшов ще один гравець, а рядок
            // на кожного перетворив би лог на шум рівно там, де адмін і мав
            // би прочитати цю єдину фразу.
            if (!s_WarnedNoBase)
            {
                s_WarnedNoBase = true;
                OZ_Log.Warn("factions: no faction is marked BaseFaction in OZ_Factions.json - nobody gets a base faction, and spawn zones fall back to staging");
            }
            return;
        }

        OZ_Factions.SetBaseOf(uid, slug);
        OZ_Log.Info("factions: " + uid + " joins the Zone as \"" + slug + "\"");
    }

    private static bool s_WarnedNoBase = false;

    // FactionShort І FactionCount ТУТ БІЛЬШЕ НЕМАЄ (2026-09-06). Обидва
    // перекривали контракт ядра, якого не питав НІХТО в усій серії, і поки
    // перекриття стояли, ядро не могло прибрати їх зі свого OZ_IdentityService:
    // Enforce не пробачає override методу, якого в базі вже немає. Коротка
    // позначка лишається доступною тим, хто залежить від цього мода явно
    // (OZ_Factions.ShortOf), а лічильник фракцій -- OZ_Factions.Count().
    override int FactionColor(string id, int alpha)
    {
        return OZ_Factions.ColorARGB(id, alpha);
    }

    override void FactionIds(out array<string> outIds)
    {
        OZ_Factions.Ids(outIds);
    }

    override string Stand(string a, string b)
    {
        return OZ_Factions.Stand(a, b);
    }

    override bool AreHostile(string a, string b)
    {
        return OZ_Factions.AreHostile(a, b);
    }

    override bool AreFriendly(string a, string b)
    {
        return OZ_Factions.AreFriendly(a, b);
    }

    override string RankOf(string uid)
    {
        return OZ_Roles.RankOf(uid);
    }

    override string FRankOf(string uid)
    {
        return OZ_Roles.FRankOf(uid);
    }

    override bool IsLeader(string uid)
    {
        return OZ_Roles.IsLeader(uid);
    }

    override bool HasPost(string uid, string post)
    {
        return OZ_Roles.HasPost(uid, post);
    }

    override bool Stale()
    {
        return OZ_Roles.Stale();
    }

    override bool PendingInvite(string uid, out string factionId, out string fromName)
    {
        factionId = "";
        fromName  = "";

        OZ_FactionInvite inv = OZ_FactionInvites.Pending(uid);
        if (!inv)
            return false;

        factionId = inv.Faction;
        fromName  = inv.FromName;
        return true;
    }

    override string SeenRankName(string uid)
    {
        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return "";
        return OZ_RoleNames.Of(v.Rank);
    }

    override void SeenTraitNames(string uid, out array<string> outNames)
    {
        if (!outNames)
            return;

        OZ_RoleView v = OZ_Roles.Seen(uid);
        if (!v)
            return;

        for (int i = 0; i < v.Traits.Count(); i++)
            outNames.Insert(OZ_RoleNames.Of(v.Traits[i]));
    }

    // Знімок із ДАНОГО запису -- поля Seen* саме його файла, а не OZ_Roles.Seen
    // і не живий файл акаунта: для замороженого покоління обидва відповідали
    // б про нове життя. Назви -- тим самим OZ_RoleNames, що й вище.
    override string SeenRankNameIn(OZ_PlayerData d)
    {
        if (!d)
            return "";
        return OZ_RoleNames.Of(d.SeenRank);
    }

    // Порожній масив, а не null, коли викликач прийшов без свого: так
    // обіцяє договір ядра. SeenTraits у старому файлі може не бути зовсім --
    // тоді й міток немає.
    override void SeenTraitNamesIn(OZ_PlayerData d, out array<string> outNames)
    {
        if (!outNames)
            outNames = new array<string>();

        if (!d || !d.SeenTraits)
            return;

        for (int i = 0; i < d.SeenTraits.Count(); i++)
            outNames.Insert(OZ_RoleNames.Of(d.SeenTraits[i]));
    }

    override string RankName(string uid)
    {
        return OZ_RoleNames.Of(OZ_Roles.RankOf(uid));
    }

    override void TraitNames(string uid, out array<string> outNames)
    {
        if (!outNames)
            return;

        OZ_RoleView v = OZ_Roles.Of(uid);
        if (!v)
            return;

        for (int i = 0; i < v.Traits.Count(); i++)
            outNames.Insert(OZ_RoleNames.Of(v.Traits[i]));
    }
}
