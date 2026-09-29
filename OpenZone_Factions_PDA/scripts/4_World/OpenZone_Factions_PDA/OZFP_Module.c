// Серверна половина склейки: одне підключення до чужої точки розширення.
//
// Сказати реєстру сторінок ядра, хто відповідає за «Фракцію». Більше нічого:
// правила фракцій живуть у моді фракцій, пристрій і його доступи -- у КПК, а
// тут лише знайомство між ними.

class OZFP_Const
{
    // Той самий рядок, що й був у КПК. Не змінюємо: id сторінки лежить у
    // профілях пристроїв на живих серверах, і перейменування зробило б їх
    // мовчки неправильними.
    static const string PAGE_FACTION = "faction";
}

[CF_RegisterModule(OZFP_Module)]
class OZFP_Module : CF_ModuleWorld
{
    override void OnInit()
    {
        super.OnInit();
        EnableMissionStart();
    }

    override void OnMissionStart(Class sender, CF_EventArgs args)
    {
        super.OnMissionStart(sender, args);

        if (!GetGame().IsServer())
            return;

        OZ_PageRegistry.Register(OZFP_Const.PAGE_FACTION,
                                 "#STR_OZ_PAGE_FACTION",
                                 "set:oz_pda image:faction",
                                 new OZ_PdaHandlerFaction());

        // Штовхати зміну ролі в КПК -- теж знайомство двох модів, і жило воно
        // в самому КПК: рядок називав OZ_RoleNotify й OZ_PdaRolePush, тобто
        // два класи, яких без мода фракцій не існує, -- і КПК через нього не
        // компілювався без нього взагалі. Місце йому тут.
        OZ_RoleNotify.On().Insert(OZ_PdaRolePush.Changed);

        // ВКЛАДКА НА ПРИЛАДАХ -- ПРОПОЗИЦІЄЮ, а не рядком у профілях КПК.
        //
        // Поставочні профілі КПК вкладки "faction" не несуть (КПК мусить
        // стояти без цього мода), а реєстрація вище лише каже, ХТО обслуговує
        // сторінку. На свіжому сервері з фракціями вкладки не було ніде, доки
        // адмін не вписував її руками в Profiles.json. Пропозиція ставить її
        // поруч із контактами кожному профілю, де контакти є, -- один раз:
        // прибрана адміном, вона назад не приходить (OZ_PdaProfiles.OfferPage).
        OZ_PdaProfiles.OfferPage(OZFP_Const.PAGE_FACTION, "contacts");

        OZ_Log.Info("faction page registered in the pda");
    }
}
