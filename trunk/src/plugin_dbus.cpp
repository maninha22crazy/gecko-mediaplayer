/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
        
#include "plugin_dbus.h"
                
static DBusHandlerResult filter_func(DBusConnection * connection,
                                             DBusMessage * message, void *user_data)
{

    const gchar *sender;
    const gchar *destination;
    int message_type;
    gchar *s = NULL;
    DBusError error;
    DBusMessage *reply_message;
    gchar *path;
    nsPluginInstance *instance;
    ListItem *item;
    gchar *arg[10];
    gint i;
    GRand *rand;
    gchar *tmp;
         	
    message_type = dbus_message_get_type(message);
    sender = dbus_message_get_sender(message);
    destination = dbus_message_get_destination(message);

    printf("path=%s; interface=%s; member=%s; data=%s\n",
           dbus_message_get_path(message),
           dbus_message_get_interface(message), dbus_message_get_member(message), s);
	
    instance = (nsPluginInstance *)user_data;
    path = instance->path;
    // printf("userdata = %s\n",path);
    	
//    if (dbus_message_get_path(message) != NULL && g_ascii_strcasecmp(dbus_message_get_path(message),path) == 0) {
    if (dbus_message_get_path(message) != NULL && is_valid_path(instance, dbus_message_get_path(message))) {
	
	//printf("Path matched %s\n", dbus_message_get_path(message));
        if (message_type == DBUS_MESSAGE_TYPE_SIGNAL) {
            if (g_ascii_strcasecmp(dbus_message_get_member(message),"Ready") == 0) {
                dbus_error_init(&error);
                if (dbus_message_get_args(message, &error, DBUS_TYPE_INT32, &i, DBUS_TYPE_INVALID)) {
                    item = list_find_by_controlid(instance->playlist,i);
                    if (item != NULL) {
                        list_mark_controlid_ready(instance->playlist,i);
                    } else {
                        // printf("Control id not found\n");
                    }
                } else {
                    dbus_error_free(&error);
                }
                
                instance->playerready = TRUE;
                instance->cache_size = request_int_value(instance, item, "GetCacheSize");
                // printf("cache size = %i\n",instance->cache_size);
                return DBUS_HANDLER_RESULT_HANDLED;
            }

            if (g_ascii_strcasecmp(dbus_message_get_member(message),"RequestById") == 0) {
                dbus_error_init(&error);
                if (dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &s, DBUS_TYPE_INVALID)) {
                    printf("Got id %s\n",s);
                    item = list_find_by_id(instance->playlist,(gint)g_strtod(s,NULL));
                    item->play = TRUE;
                    printf("id %s has url of %s\n",s,item->src);
                    printf("id %s has newwindow = %i\n",s,item->newwindow);
                    if(item->newwindow == 0) {
                        send_signal_with_boolean(instance,item, "SetShowControls",TRUE);
                        if (item->streaming) {
                            send_signal_with_string(instance, item, "Open",item->src);
                        } else {
                            NPN_GetURLNotify(instance->mInstance,item->src, NULL, item);
                        }
                    } else {
                        i = 0;
                        // generate a random controlid
                        rand = g_rand_new();
                        item->controlid = g_rand_int_range(rand,0,65535);
                        g_rand_free(rand);
                        tmp = g_strdup_printf("/control/%i",item->controlid);
                        g_strlcpy(item->path,tmp,1024);
                        g_free(tmp);
                        
                        arg[i++] = g_strdup("gnome-mplayer");
                        arg[i++] = g_strdup_printf("--controlid=%i",item->controlid);
                        arg[i] = NULL;
                        g_spawn_async(NULL, arg, NULL,
                                           G_SPAWN_SEARCH_PATH,
                                           NULL, NULL, NULL, NULL);
                        NPN_GetURLNotify(instance->mInstance,item->src, NULL, item);                        
                    }
                    instance->lastopened->played = TRUE;
                    item->requested = TRUE;
                    instance->lastopened = item;
                } else {
                    dbus_error_free(&error);
                }
                return DBUS_HANDLER_RESULT_HANDLED;
            }
            
            if (g_ascii_strcasecmp(dbus_message_get_member(message),"Next") == 0) {
                if (instance->lastopened != NULL && instance->lastopened->loop == FALSE) {
                    instance->lastopened->played = TRUE;
                    item = list_find_next_playable(instance->playlist);
                } else {
                    if (instance->lastopened != NULL && instance->lastopened->loop == TRUE) {
                        if (instance->lastopened->loopcount < 0) {
                            item = instance->lastopened;
                        } else if(instance->lastopened->loopcount > 0 ) {
                            instance->lastopened->loopcount--;
                            item = instance->lastopened;
                        } else {
                            // listcount = 0
                            instance->lastopened->loop = FALSE;
                            item = list_find_next_playable(instance->playlist); 
                        }
                    }
                }
                if (item != NULL) {
                    if(item->newwindow == 0) {
                        open_location(instance,item,TRUE);
                    } else {
                        i = 0;
                        arg[i++] = g_strdup("gnome-mplayer");
                        arg[i++] = g_strdup_printf("--controlid=%i",instance->controlid);
                        arg[i++] = g_strdup(item->src);
                        arg[i] = NULL;
                        g_spawn_async(NULL, arg, NULL,
                                      G_SPAWN_SEARCH_PATH,
                                      NULL, NULL, NULL, NULL);
                        item->opened = TRUE;
                        instance->lastopened = item;
                    }
                } 
                return DBUS_HANDLER_RESULT_HANDLED;
            }
        }		
    } else {
        printf("path didn't match path = %s\n",path); 
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}       

DBusConnection *dbus_hookup(nsPluginInstance *instance) {
        DBusConnection *connection;
        DBusError dberror;
        DBusBusType type = DBUS_BUS_SESSION;
                
        dbus_error_init(&dberror);
        connection = dbus_bus_get(type, &dberror);
        dbus_connection_setup_with_g_main(connection, NULL);
        
        dbus_bus_add_match(connection, "type='signal',interface='com.gecko.mediaplayer'", NULL);
        dbus_connection_add_filter(connection, filter_func,instance,NULL); 
        
        printf("DBUS connection created\nListening to path %s\n",instance->path);
        
        return connection;
}

DBusConnection *dbus_unhook(DBusConnection *connection, nsPluginInstance *instance) {
    
    dbus_connection_flush(connection);
    dbus_connection_remove_filter(connection,filter_func, instance);
    dbus_connection_unref(connection);
    
    return NULL;   
}

void open_location(nsPluginInstance *instance, ListItem *item, gboolean uselocal) {
    DBusMessage *message;
    const char *file;
    const char *id;
    char *path;
    GError *error = NULL;
    gchar *argvn[255];
    gint arg = 0;
    gint ok;
    
    list_dump(instance->playlist);
    //printf("Sending Open %s to connection %p\n",file, instance->connection);
    
    if (!(instance->player_launched)) {
        if (!item->opened) {
            if (uselocal) {
                file = g_strdup(item->local);
            } else {
                file = g_strdup(item->src);
            }
            
            //printf("launching gnome-mplayer from Open with id = %i\n",instance->controlid);
            argvn[arg++] = g_strdup_printf("gnome-mplayer");
            argvn[arg++] = g_strdup_printf("--controlid=%i",instance->controlid);
            argvn[arg++] = g_strdup_printf("%s",file);
            argvn[arg] = g_strdup("");
            argvn[arg + 1] = NULL;
            instance->playerready = FALSE;
            ok = g_spawn_async(NULL, argvn, NULL,
                            G_SPAWN_SEARCH_PATH,
                            NULL, NULL, NULL, &error);
    
            if (ok)	instance->player_launched = TRUE;
            item->opened = TRUE;
            instance->lastopened = item;    
        }        
        
        return;     
           
    } else {
    
        while (!(instance->playerready)) {
            g_main_context_iteration(NULL,FALSE);   
        }
        
        if (item->controlid != 0) {
            while (!(item->playerready)) {
                g_main_context_iteration(NULL,FALSE);   
            }
        }
    }
    
    if (!item->opened) {
        if (uselocal) {
            file = g_strdup(item->local);
        } else {
            file = g_strdup(item->src);
        }
        
        if (strlen(item->path) > 0) {
            path = item->path;
        } else {
            path = instance->path;
        }
        
        //printf("Sending Open %s to connection %p\n",file, instance->connection);
        if (item->hrefid == 0) {
            message = dbus_message_new_signal(path,"com.gnome.mplayer","Open");
            dbus_message_append_args(message, DBUS_TYPE_STRING, &file, DBUS_TYPE_INVALID);
            dbus_connection_send(instance->connection,message,NULL);
            dbus_message_unref(message);
        } else {
            // ok, not done here yet, may need a new window for Apple HD video
            id = g_strdup_printf("%i",item->hrefid);
            message = dbus_message_new_signal(path,"com.gnome.mplayer","OpenButton");
            dbus_message_append_args(message, DBUS_TYPE_STRING, &file, 
                                     DBUS_TYPE_STRING, &id,
                                     DBUS_TYPE_INVALID);
            dbus_connection_send(instance->connection,message,NULL);
            dbus_message_unref(message);
        }                    
        item->opened = TRUE;
        instance->lastopened = item;    
    }    
}

void resize_window(nsPluginInstance *instance, ListItem *item, gint x, gint y) {
    DBusMessage *message;
    gchar *path;
    
    if (instance == NULL) return;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }

    if (instance->playerready) {
        if (instance->connection != NULL) {
            message = dbus_message_new_signal(path,"com.gnome.mplayer", "ResizeWindow");
            dbus_message_append_args(message, DBUS_TYPE_INT32, &x, DBUS_TYPE_INT32, &y, DBUS_TYPE_INVALID);
            dbus_connection_send(instance->connection,message,NULL);
            dbus_message_unref(message);
        }
    }
}


void send_signal(nsPluginInstance *instance, ListItem *item, gchar *signal) {
    DBusMessage *message;
    const char *localsignal;
    gchar *path;
    
    // printf("Sending %s to connection %p\n", signal, instance->connection);
    if (instance == NULL) return;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localsignal = g_strdup(signal);
        message = dbus_message_new_signal(path,"com.gnome.mplayer", localsignal);
        dbus_connection_send(instance->connection,message,NULL);
        dbus_message_unref(message);
    }
    
}

void send_signal_when_ready(nsPluginInstance *instance, ListItem *item, gchar *signal) {
    DBusMessage *message;
    const char *localsignal;
    gchar *path;
    
    if (instance == NULL) return;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->player_launched) {
        while (!(instance->playerready)) {
            g_main_context_iteration(NULL,FALSE);   
        }
            
        if (instance->playerready && instance->connection != NULL) {
            //printf("Sending %s to connection %p\n", signal, instance->connection);
            localsignal = g_strdup(signal);
            message = dbus_message_new_signal(path,"com.gnome.mplayer", localsignal);
            dbus_connection_send(instance->connection,message,NULL);
            dbus_message_unref(message);
            //printf("Sent %s to connection %p\n", signal, instance->connection);
            while (g_main_context_pending(NULL)) {
                g_main_context_iteration(NULL,FALSE);   
            }    
        }
    } 
}

void send_signal_with_string(nsPluginInstance *instance, ListItem *item, gchar *signal, gchar *str) {
    DBusMessage *message;
    const char *localsignal;
    const char *localstr;
    gchar *path;
    
    //printf("Sending %s to connection %p\n", signal, instance->connection);
    if (instance == NULL) return;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localsignal = g_strdup(signal);
        localstr = g_strdup(str);
        message = dbus_message_new_signal(path,"com.gnome.mplayer", localsignal);
        dbus_message_append_args(message, DBUS_TYPE_STRING, &localstr, DBUS_TYPE_INVALID);
        dbus_connection_send(instance->connection,message,NULL);
        dbus_message_unref(message);
    }
    
}

void send_signal_with_double(nsPluginInstance *instance, ListItem *item, gchar *signal, gdouble dbl) {
    DBusMessage *message;
    const char *localsignal;
    gchar *path;
    
    //printf("Sending %s to connection %p\n", signal, instance->connection);
    if (instance == NULL) return;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localsignal = g_strdup(signal);
        message = dbus_message_new_signal(path,"com.gnome.mplayer", localsignal);
        dbus_message_append_args(message, DBUS_TYPE_DOUBLE, &dbl, DBUS_TYPE_INVALID);
        dbus_connection_send(instance->connection,message,NULL);
        dbus_message_unref(message);
    }
    
}

void send_signal_with_boolean(nsPluginInstance *instance, ListItem *item, gchar *signal, gboolean boolean) {
    DBusMessage *message;
    const char *localsignal;
    gchar *path;
    
    //printf("Sending %s to connection %p\n", signal, instance->connection);
    if (instance == NULL) return;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localsignal = g_strdup(signal);
        message = dbus_message_new_signal(path,"com.gnome.mplayer", localsignal);
        dbus_message_append_args(message, DBUS_TYPE_BOOLEAN, &boolean, DBUS_TYPE_INVALID);
        dbus_connection_send(instance->connection,message,NULL);
        dbus_message_unref(message);
    }
    
}

gboolean request_boolean_value(nsPluginInstance *instance, ListItem *item, gchar *member) {
    DBusMessage *message;
    DBusMessage *replymessage;
    const gchar *localmember;
    DBusError error;
    gboolean result = FALSE;    
    gchar *path;
    
    //printf("Requesting %s to connection %p\n", member, instance->connection);
    if (instance == NULL) return result;

    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localmember = g_strdup(member);
        message = dbus_message_new_method_call("com.gnome.mplayer", path, "com.gnome.mplayer", localmember);
        dbus_error_init(&error);
        replymessage = dbus_connection_send_with_reply_and_block(instance->connection,message, -1 ,&error);
        if (dbus_error_is_set(&error)) {
            printf("Error message = %s\n",error.message);
        }
        dbus_message_get_args(replymessage, &error, DBUS_TYPE_BOOLEAN, &result, DBUS_TYPE_INVALID);        
        dbus_message_unref(message);
        dbus_message_unref(replymessage);
    }
    
    return result;
}

gdouble request_double_value(nsPluginInstance *instance, ListItem *item, gchar *member) {
    DBusMessage *message;
    DBusMessage *replymessage;
    const gchar *localmember;
    DBusError error;
    gdouble result = 0.0;    
    gchar *path;
    
    //printf("Requesting %s to connection %p\n", member, instance->connection);
    if (instance == NULL) return result;
    
    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localmember = g_strdup(member);
        message = dbus_message_new_method_call("com.gnome.mplayer", path, "com.gnome.mplayer", localmember);
        dbus_error_init(&error);
        replymessage = dbus_connection_send_with_reply_and_block(instance->connection,message, -1 ,&error);
        if (dbus_error_is_set(&error)) {
            printf("Error message = %s\n",error.message);
        }
        dbus_message_get_args(replymessage, &error, DBUS_TYPE_DOUBLE, &result, DBUS_TYPE_INVALID);
        dbus_message_unref(message);
        dbus_message_unref(replymessage);
    }
    
    return result;
}

gint request_int_value(nsPluginInstance *instance, ListItem *item, gchar *member) {
    DBusMessage *message;
    DBusMessage *replymessage;
    const gchar *localmember;
    DBusError error;
    gint result = 0;    
    gchar *path;
    
    //printf("Requesting %s to connection %p\n", member, instance->connection);
    if (instance == NULL) return result;

    if (item != NULL && strlen(item->path) > 0) {
        path = item->path;
    } else {
        path = instance->path;
    }
    
    if (instance->playerready && instance->connection != NULL) {
        localmember = g_strdup(member);
        message = dbus_message_new_method_call("com.gnome.mplayer", path, "com.gnome.mplayer", localmember);
        dbus_error_init(&error);
        replymessage = dbus_connection_send_with_reply_and_block(instance->connection,message, -1 ,&error);
        if (dbus_error_is_set(&error)) {
            printf("Error message = %s\n",error.message);
        }
        dbus_message_get_args(replymessage, &error, DBUS_TYPE_INT32, &result, DBUS_TYPE_INVALID);
        dbus_message_unref(message);
        dbus_message_unref(replymessage);
    }
    
    return result;
}

gboolean is_valid_path(nsPluginInstance *instance, const char *message) {
    gboolean result = FALSE;
    ListItem *item;
    GList *iter;   
    
    if (instance == NULL) return result;
    
    if (g_ascii_strcasecmp(message,instance->path) == 0) {
        
        result = TRUE;
        
    } else {
    
        if (instance->playlist != NULL) {
            for (iter = instance->playlist; iter !=NULL; iter = g_list_next(iter)) {
                item = (ListItem *)iter->data;
                if (item != NULL) {
                    if (g_ascii_strcasecmp(message,item->path) == 0) {
                        result = TRUE;
                    }
                }
            }
        }
        
    }
    
    return result; 
}
