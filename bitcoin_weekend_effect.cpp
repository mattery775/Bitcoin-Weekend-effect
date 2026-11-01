#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <map>

using namespace std;
using namespace std::chrono;

// --------------------------------------------------
// 1. DATENSTRUKTUREN
// --------------------------------------------------

struct BitcoinDay {
    sys_days date;
    double close;
};

struct Statistics {

    int count = 0;
    int positiveDays = 0;

    double meanReturn = 0.0;
    double squaredDeviationSum = 0.0;
    double compoundedFactor = 1.0;

    // Tagesrendite zur Statistik hinzufuegen
    void add(double dailyReturn) {

        count++;

        // Durchschnitt und Varianz nach Welford
        double difference = dailyReturn - meanReturn;

        meanReturn += difference / count;

        double differenceAfterUpdate =
            dailyReturn - meanReturn;

        squaredDeviationSum +=
            difference * differenceAfterUpdate;

        // Kumulative Rendite
        compoundedFactor *= (1.0 + dailyReturn);

        if (dailyReturn > 0.0) {
            positiveDays++;
        }
    }

    // Durchschnittliche Rendite
    double averageReturn() const {
        return meanReturn;
    }

    // Taegliche Volatilitaet
    double dailyVolatility() const {

        if (count < 2) {
            return 0.0;
        }

        double variance =
            squaredDeviationSum / (count - 1);

        return sqrt(max(0.0, variance));
    }

    // Annualisierte Volatilitaet
    double annualizedVolatility(
        double observationsPerYear
    ) const {

        return dailyVolatility() *
            sqrt(observationsPerYear);
    }

    // Annualisierte Sharpe Ratio
    // Risikofreier Zinssatz = 0
    double annualizedSharpeRatio(
        double observationsPerYear
    ) const {

        double volatility = dailyVolatility();

        if (count < 2 || volatility <= 0.0) {
            return 0.0;
        }

        return (meanReturn / volatility) *
            sqrt(observationsPerYear);
    }

    // Kumulative Rendite
    double cumulativeReturn() const {
        return compoundedFactor - 1.0;
    }

    // Anteil positiver Tage
    double positivePercentage() const {

        if (count == 0) {
            return 0.0;
        }

        return static_cast<double>(positiveDays)
            / count * 100.0;
    }
};

// Statistiken fuer ein einzelnes Kalenderjahr
struct YearStatistics {

    Statistics weekdays;
    Statistics weekends;

    int skippedGaps = 0;
};

// --------------------------------------------------
// 2. HILFSFUNKTIONEN
// --------------------------------------------------

// Entfernt Leerzeichen
string clean(string text) {

    const string whitespace = " \t\r\n";

    size_t first =
        text.find_first_not_of(whitespace);

    if (first == string::npos) {
        return "";
    }

    size_t last =
        text.find_last_not_of(whitespace);

    return text.substr(
        first,
        last - first + 1
    );
}

// Findet eine CSV-Spalte anhand ihres Namens
int findColumn(
    const vector<string>& headers,
    const string& name
) {

    for (size_t i = 0; i < headers.size(); i++) {

        if (clean(headers[i]) == name) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

// Wandelt YYYY-MM-DD in ein Datum um
bool parseDate(
    const string& text,
    sys_days& result
) {

    if (text.size() < 10) {
        return false;
    }

    try {

        int yearValue =
            stoi(text.substr(0, 4));

        unsigned monthValue =
            static_cast<unsigned>(
                stoi(text.substr(5, 2))
            );

        unsigned dayValue =
            static_cast<unsigned>(
                stoi(text.substr(8, 2))
            );

        year_month_day date{
            year{yearValue},
            month{monthValue},
            day{dayValue}
        };

        if (!date.ok()) {
            return false;
        }

        result = sys_days{date};

        return true;
    }
    catch (...) {

        return false;
    }
}

// Formatiert ein Datum als YYYY-MM-DD
string formatDate(sys_days date) {

    year_month_day ymd{date};

    ostringstream output;

    output << setfill('0')
           << setw(4)
           << static_cast<int>(ymd.year()) << "-"
           << setw(2)
           << static_cast<unsigned>(ymd.month()) << "-"
           << setw(2)
           << static_cast<unsigned>(ymd.day());

    return output.str();
}

// --------------------------------------------------
// 3. CSV-DATEI EINLESEN
// --------------------------------------------------

vector<BitcoinDay> loadBitcoinData(
    const string& filename
) {

    vector<BitcoinDay> data;

    ifstream file(filename);

    if (!file.is_open()) {

        cerr << "Fehler: CSV-Datei konnte nicht "
             << "geoeffnet werden: "
             << filename << endl;

        return data;
    }

    string line;

    // Kopfzeile einlesen
    if (!getline(file, line)) {

        cerr << "Fehler: CSV-Datei ist leer."
             << endl;

        return data;
    }

    stringstream headerStream(line);

    vector<string> headers;

    string field;

    while (getline(headerStream, field, ',')) {
        headers.push_back(clean(field));
    }

    // Eventuelles UTF-8-BOM entfernen
    if (!headers.empty() &&
        headers[0].size() >= 3 &&
        static_cast<unsigned char>(headers[0][0]) == 0xEF &&
        static_cast<unsigned char>(headers[0][1]) == 0xBB &&
        static_cast<unsigned char>(headers[0][2]) == 0xBF) {

        headers[0].erase(0, 3);
    }

    // Benoetigte Spalten finden
    int dateColumn =
        findColumn(headers, "Date");

    int closeColumn =
        findColumn(headers, "Close");

    if (dateColumn == -1 || closeColumn == -1) {

        cerr << "Fehler: Die Spalten Date und Close "
             << "wurden nicht gefunden."
             << endl;

        return data;
    }

    int requiredColumn =
        max(dateColumn, closeColumn);

    // Datenzeilen einlesen
    while (getline(file, line)) {

        stringstream rowStream(line);

        vector<string> values;

        while (getline(rowStream, field, ',')) {
            values.push_back(clean(field));
        }

        if (static_cast<int>(values.size())
            <= requiredColumn) {

            continue;
        }

        sys_days date;

        if (!parseDate(values[dateColumn], date)) {
            continue;
        }

        try {

            size_t parsedCharacters = 0;

            double close =
                stod(
                    values[closeColumn],
                    &parsedCharacters
                );

            if (
                parsedCharacters !=
                    values[closeColumn].size() ||
                !isfinite(close) ||
                close <= 0.0
            ) {
                continue;
            }

            data.push_back({date, close});
        }
        catch (...) {

            continue;
        }
    }

    file.close();

    // Chronologisch sortieren
    sort(
        data.begin(),
        data.end(),

        [](const BitcoinDay& a,
           const BitcoinDay& b) {

            return a.date < b.date;
        }
    );

    // Doppelte Datumswerte entfernen
    vector<BitcoinDay> uniqueData;

    for (const auto& item : data) {

        if (
            uniqueData.empty() ||
            uniqueData.back().date != item.date
        ) {

            uniqueData.push_back(item);
        }
    }

    return uniqueData;
}

// --------------------------------------------------
// 4. STATISTIKEN AUSGEBEN
// --------------------------------------------------

void printStatistics(
    const string& title,
    const Statistics& stats,
    double observationsPerYear
) {

    cout << "\n" << title << endl;

    cout << "---------------------------------------------"
         << endl;

    if (stats.count == 0) {

        cout << "Keine Daten vorhanden." << endl;

        return;
    }

    cout << fixed << setprecision(4);

    cout << "Anzahl Renditen:             "
         << stats.count << endl;

    cout << "Durchschnittliche Rendite:   "
         << stats.averageReturn() * 100.0
         << " % pro Tag" << endl;

    cout << "Kumulative Rendite:          "
         << stats.cumulativeReturn() * 100.0
         << " %" << endl;

    cout << "Taegliche Volatilitaet:      "
         << stats.dailyVolatility() * 100.0
         << " %" << endl;

    cout << "Annualisierte Volatilitaet:  "
         << stats.annualizedVolatility(
                observationsPerYear
            ) * 100.0
         << " %" << endl;

    cout << "Sharpe Ratio (annualisiert): "
         << stats.annualizedSharpeRatio(
                observationsPerYear
            )
         << " (risikofreier Zins = 0)"
         << endl;

    cout << "Positive Tage:               "
         << stats.positivePercentage()
         << " %" << endl;
}

// --------------------------------------------------
// 5. HAUPTPROGRAMM
// --------------------------------------------------

int main() {

    // Anzahl erwarteter Renditebeobachtungen pro Jahr

    const double WEEKDAYS_PER_YEAR =
        365.0 * 5.0 / 7.0;

    const double WEEKENDS_PER_YEAR =
        365.0 * 2.0 / 7.0;

    cout << "================================================="
         << endl;

    cout << "Bitcoin Weekend Effect - "
         << "A Quantitative Market Analysis"
         << endl;

    cout << "================================================="
         << endl;

    // CSV-Datei finden
    string filename = "data/btc_v_d.csv";

    ifstream testFile(filename);

    if (!testFile.is_open()) {

        filename = "data/btc_v_d.csv";
    }
    else {

        testFile.close();
    }

    // Bitcoin-Daten laden
    vector<BitcoinDay> data =
        loadBitcoinData(filename);

    if (data.size() < 2) {

        cerr << "Nicht genuegend Bitcoin-Daten vorhanden."
             << endl;

        return 1;
    }

    // Gesamtstatistiken erstellen
    Statistics weekdays;
    Statistics weekends;

    int skippedGaps = 0;

    // Jahresstatistiken
    map<int, YearStatistics> yearlyStatistics;

    // --------------------------------------------------
    // 6. TAEGLICHE RENDITEN BERECHNEN
    // --------------------------------------------------

    for (size_t i = 1; i < data.size(); i++) {

        // Nur aufeinanderfolgende Kalendertage verwenden
        if (
            (data[i].date -
             data[i - 1].date).count() != 1
        ) {

            skippedGaps++;

            int gapYear =
                static_cast<int>(
                    year_month_day{
                        data[i].date
                    }.year()
                );

            yearlyStatistics[gapYear].skippedGaps++;

            continue;
        }

        // Tagesrendite berechnen
        double dailyReturn =
            (data[i].close /
             data[i - 1].close) - 1.0;

        // Wochentag bestimmen
        weekday dayOfWeek{data[i].date};

        // Sonntag = 0, Samstag = 6
        unsigned day =
            dayOfWeek.c_encoding();

        // Jahr der Rendite bestimmen
        int currentYear =
            static_cast<int>(
                year_month_day{
                    data[i].date
                }.year()
            );

        // Wochenende oder Wochentag zuordnen
        if (day == 0 || day == 6) {

            weekends.add(dailyReturn);

            yearlyStatistics[currentYear]
                .weekends.add(dailyReturn);
        }
        else {

            weekdays.add(dailyReturn);

            yearlyStatistics[currentYear]
                .weekdays.add(dailyReturn);
        }
    }

    // --------------------------------------------------
    // 7. DATENGRUNDLAGE AUSGEBEN
    // --------------------------------------------------

    cout << "\nDATENGRUNDLAGE" << endl;

    cout << "---------------------------------------------"
         << endl;

    cout << "Geladene Kurstage: "
         << data.size() << endl;

    cout << "Erster Kurstag: "
         << formatDate(data.front().date) << endl;

    cout << "Letzter Kurstag: "
         << formatDate(data.back().date) << endl;

    cout << "Uebersprungene Datenluecken: "
         << skippedGaps << endl;

    // --------------------------------------------------
    // 8. GESAMTSTATISTIKEN
    // --------------------------------------------------

    printStatistics(
        "WOCHENTAGE (MONTAG BIS FREITAG)",
        weekdays,
        WEEKDAYS_PER_YEAR
    );

    printStatistics(
        "WOCHENENDE (SAMSTAG UND SONNTAG)",
        weekends,
        WEEKENDS_PER_YEAR
    );

    // --------------------------------------------------
    // 9. DIREKTER GESAMTVERGLEICH
    // --------------------------------------------------

    cout << "\nDIREKTER VERGLEICH" << endl;

    cout << "---------------------------------------------"
         << endl;

    if (
        weekdays.count > 0 &&
        weekends.count > 0
    ) {

        double difference =
            (weekends.averageReturn()
             - weekdays.averageReturn()) * 100.0;

        cout << fixed << setprecision(4);

        cout << "Differenz der durchschnittlichen "
             << "Tagesrendite: "
             << difference
             << " Prozentpunkte"
             << endl;

        cout << "\nRISIKOBEREINIGTER VERGLEICH"
             << endl;

        cout << "Sharpe Ratio Wochentage: "
             << weekdays.annualizedSharpeRatio(
                    WEEKDAYS_PER_YEAR
                )
             << endl;

        cout << "Sharpe Ratio Wochenende: "
             << weekends.annualizedSharpeRatio(
                    WEEKENDS_PER_YEAR
                )
             << endl;

        if (
            weekdays.annualizedSharpeRatio(
                WEEKDAYS_PER_YEAR
            ) >
            weekends.annualizedSharpeRatio(
                WEEKENDS_PER_YEAR
            )
        ) {

            cout << "Ergebnis: Wochentage haben "
                 << "die bessere risikobereinigte Rendite."
                 << endl;
        }
        else {

            cout << "Ergebnis: Wochenende hat "
                 << "die bessere risikobereinigte Rendite."
                 << endl;
        }
    }
    else {

        cout << "Vergleich nicht moeglich."
             << endl;
    }

    // --------------------------------------------------
    // 10. JAHR-FUER-JAHR-ANALYSE
    // --------------------------------------------------

    cout << "\n\n================================================="
         << endl;

    cout << "JAHR-FUER-JAHR-ANALYSE" << endl;

    cout << "================================================="
         << endl;

    for (const auto& entry : yearlyStatistics) {

        int year = entry.first;

        const YearStatistics& stats =
            entry.second;

        cout << "\n\n#############################################"
             << endl;

        cout << "ANALYSEJAHR: "
             << year << endl;

        cout << "#############################################"
             << endl;

        cout << "Uebersprungene Datenluecken in diesem Jahr: "
             << stats.skippedGaps
             << endl;

        // Wochentage
        printStatistics(
            "WOCHENTAGE (MONTAG BIS FREITAG) - "
            + to_string(year),

            stats.weekdays,
            WEEKDAYS_PER_YEAR
        );

        // Wochenende
        printStatistics(
            "WOCHENENDE (SAMSTAG UND SONNTAG) - "
            + to_string(year),

            stats.weekends,
            WEEKENDS_PER_YEAR
        );

        // --------------------------------------------------
        // JAHRESVERGLEICH
        // --------------------------------------------------

        cout << "\nJAHRESVERGLEICH "
             << year << endl;

        cout << "---------------------------------------------"
             << endl;

        if (
            stats.weekdays.count > 0 &&
            stats.weekends.count > 0
        ) {

            double yearlyDifference =
                (stats.weekends.averageReturn()
                 - stats.weekdays.averageReturn()) * 100.0;

            cout << fixed << setprecision(4);

            cout << "Differenz der durchschnittlichen "
                 << "Tagesrendite: "
                 << yearlyDifference
                 << " Prozentpunkte"
                 << endl;

            // Sharpe Ratios berechnen
            double weekdaySharpe =
                stats.weekdays.annualizedSharpeRatio(
                    WEEKDAYS_PER_YEAR
                );

            double weekendSharpe =
                stats.weekends.annualizedSharpeRatio(
                    WEEKENDS_PER_YEAR
                );

            cout << "\nRISIKOBEREINIGTER VERGLEICH"
                 << endl;

            cout << "Sharpe Ratio Wochentage: "
                 << weekdaySharpe << endl;

            cout << "Sharpe Ratio Wochenende: "
                 << weekendSharpe << endl;

            if (weekdaySharpe > weekendSharpe) {

                cout << "Risikobereinigt: "
                     << "Wochentage besser."
                     << endl;
            }
            else if (weekendSharpe > weekdaySharpe) {

                cout << "Risikobereinigt: "
                     << "Wochenende besser."
                     << endl;
            }
            else {

                cout << "Risikobereinigt: "
                     << "Beide Gruppen gleich."
                     << endl;
            }

            // Vergleich der durchschnittlichen Rendite
            if (yearlyDifference > 0.0) {

                cout << "Ergebnis: Das Wochenende weist "
                     << "eine hoehere durchschnittliche "
                     << "Tagesrendite auf."
                     << endl;
            }
            else if (yearlyDifference < 0.0) {

                cout << "Ergebnis: Die Wochentage weisen "
                     << "eine hoehere durchschnittliche "
                     << "Tagesrendite auf."
                     << endl;
            }
            else {

                cout << "Ergebnis: Beide Gruppen weisen "
                     << "dieselbe durchschnittliche "
                     << "Tagesrendite auf."
                     << endl;
            }
        }
        else {

            cout << "Jahresvergleich nicht moeglich: "
                 << "Nicht genuegend Renditedaten "
                 << "in beiden Gruppen."
                 << endl;
        }
    }

    // --------------------------------------------------
    // 11. ABSCHLUSS
    // --------------------------------------------------

    cout << "\nHinweis: Die Analyse ist deskriptiv. "
         << "Ein statistischer Signifikanztest "
         << "ist nicht enthalten."
         << endl;

    cout << "Die Sharpe Ratio verwendet einen "
         << "risikofreien Zinssatz von 0."
         << endl;

    cout << "Die Annualisierung beruecksichtigt "
         << "ca. 260.7 Renditen fuer Wochentage "
         << "und ca. 104.3 fuer Wochenenden pro Jahr."
         << endl;

    cout << "================================================="
         << endl;

    cout << "Analyse abgeschlossen."
         << endl;

    return 0;
}