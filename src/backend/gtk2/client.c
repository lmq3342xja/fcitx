#include <stdlib.h>
#include <dbus/dbus-glib.h>
#include "fcitx/fcitx.h"
#include "fcitx-utils/utils.h"
#include "client.h"
#include "fcitx-config/fcitx-config.h"
#include <fcitx/ime.h>

#define IC_NAME_MAX 64

struct FcitxIMClient {
    DBusGConnection* conn;
    DBusGProxy* proxy;
    DBusGProxy* icproxy;
    char icname[IC_NAME_MAX];
    int id;
};

static void FcitxIMClientCreateIC(FcitxIMClient* client);

boolean IsFcitxIMClientValid(FcitxIMClient* client)
{
    if (client == NULL)
        return false;
    if (client->proxy == NULL || client->icproxy == NULL)
        return false;
    
    return true;
}

FcitxIMClient* FcitxIMClientOpen()
{
    FcitxIMClient* client = fcitx_malloc0(sizeof(FcitxIMClient));
    GError *error = NULL;
    client->conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
    client->id = -1;
    if (client->conn == NULL)
    {
        free(client);
        return NULL;
    }
    client->proxy = dbus_g_proxy_new_for_name_owner(client->conn,
                                                    "org.fcitx.fcitx",
                                                    "/inputmethod",
                                                    "org.fcitx.fcitx",
                                                    &error);
    
    FcitxIMClientCreateIC(client);
    return client;
}

void FcitxIMClientCreateIC(FcitxIMClient* client)
{
    GError* error = NULL;
    int id = -1;
    dbus_g_proxy_call(client->proxy, "CreateIC", &error, G_TYPE_INVALID, G_TYPE_INT, &id, G_TYPE_INVALID);
    if (id >= 0)
        client->id = id;
    else
        return;
    
    sprintf(client->icname, "/inputcontext_%d", client->id);    
    
    client->icproxy = dbus_g_proxy_new_for_name_owner(client->conn,
                                                      "org.fcitx.fcitx",
                                                      client->icname,
                                                      "org.fcitx.fcitx",
                                                      &error
                                                     );
}

void FcitxIMClientClose(FcitxIMClient* client)
{
    if (client->icproxy)
    {
        dbus_g_proxy_call_no_reply(client->icproxy, "DestroyIC", G_TYPE_INVALID);
    }
    g_object_unref(client->icproxy);
    g_object_unref(client->proxy);
    free(client);
}

void FcitxIMClientFocusIn(FcitxIMClient* client)
{
    if (client->icproxy)
    {
        dbus_g_proxy_call_no_reply(client->icproxy, "FocusIn", G_TYPE_INVALID);
    }
}

void FcitxIMClientFocusOut(FcitxIMClient* client)
{
    if (client->icproxy)
    {
        dbus_g_proxy_call_no_reply(client->icproxy, "FocusIn", G_TYPE_INVALID);
    }
}

void FcitxIMClientReset(FcitxIMClient* client)
{
    if (client->icproxy)
    {
        dbus_g_proxy_call_no_reply(client->icproxy, "Reset", G_TYPE_INVALID);
    }
}

void FcitxIMClientSetCursorLocation(FcitxIMClient* client, int x, int y)
{
    if (client->icproxy)
    {
        dbus_g_proxy_call_no_reply(client->icproxy, "SetCursorLocation", G_TYPE_INT, x, G_TYPE_INT, y, G_TYPE_INVALID);
    }
}

int FcitxIMClientProcessKey(FcitxIMClient* client, uint32_t keyval, uint32_t keycode, uint32_t state, FcitxKeyEventType type, uint32_t t)
{
    int ret;
    int itype = type;
    GError* error = NULL;
    if (!dbus_g_proxy_call(client->icproxy, "ProcessKeyEvent",
                      &error,
                      G_TYPE_UINT, keyval,
                      G_TYPE_UINT, keycode,
                      G_TYPE_UINT, state,
                      G_TYPE_INT, itype,
                      G_TYPE_UINT, t,
                      G_TYPE_INVALID,
                      G_TYPE_INT, &ret,
                      G_TYPE_INVALID
                     ))
    {
        return -1;
    }
    
    
    return ret;
}

DBusGConnection* FcitxIMClientGetConnection(FcitxIMClient* imclient)
{
    return imclient->conn;
}