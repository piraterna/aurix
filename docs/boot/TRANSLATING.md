# Translating AxBoot

To translate AxBoot, copy `en_US.c` in the `common/i18n` directory and rename it (and the translation structure) to the target language code.
Before translating each string, open `common/i18n/i18n.c` and register the new language:

```c
extern struct language i18n_xxXX;

struct language_selection i18n_languages[] = {
	{
		"English", // Localized language name (eg. English, Čeština, Svenska, ...)
		"en_US", // Language code
		&i18n_enUS // reference to the language structure
	},
	/* ... */
}
```
