#pragma once

#include <Arduino.h>
#include <WebServer.h>

namespace web_storage {

// Token simple de remplacement pour templates HTML.
// key doit correspondre a une balise de type {{TOKEN}} dans le fichier source.
struct TemplateToken {
  const char* key;
  String value;
};

// Monte LittleFS et initialise l'etat interne du module.
// Retourne true si le filesystem est disponible.
bool initialize();

// Retourne true si LittleFS a ete monte avec succes.
bool isReady();

// Envoie un fichier statique au client HTTP.
// Retourne false si LittleFS est indisponible ou si le fichier est absent.
// cacheControl permet d'appliquer un en-tete HTTP Cache-Control optionnel.
bool sendStaticFile(WebServer& server,
                    const char* path,
                    const char* contentType,
                    const char* cacheControl = nullptr);

// Charge un template depuis LittleFS et applique les remplacements de tokens.
// Retourne une chaine vide si le template est introuvable ou vide.
String renderTemplate(const char* path, const TemplateToken* tokens, size_t tokenCount);

}  // namespace web_storage
