#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTextStream>
#include <QAtomicInt>
#include <QDateTime>
#include <QRegExp>
#include <QTimer>
#include <QUrlQuery>

#include <functional>

#include "Bot.h"

namespace HelloInternet
{

Bot::Bot(QAtomicInt *counter,
        int id,
        QString pollnumber,
        QString ourpick,
        QUrl cookieBaseUrl,
        QUrl voteBaseUrl
        )
    : QObject(nullptr)
    , m_voteCounter(counter)
    , m_id(id)
    , m_pollnumber(pollnumber)
    , m_ourpick(ourpick)
    , m_cookieBaseUrl(cookieBaseUrl)
    , m_voteBaseUrl(voteBaseUrl)
{
}

Bot::~Bot()
{
}

void Bot::run()
{
    Q_ASSERT(m_net == nullptr);
    if (m_net) return;

//    const qint64 key {
//        QDateTime::currentMSecsSinceEpoch()
//    };
//    const uint seed {
//        qHash(key, m_id)
//    };
//    qsrand(seed);

    //qDebug() << "[KEY / SEED] " << key << " / " << seed << endl;

    m_net = new QNetworkAccessManager(this);
    getCookie();
}

void Bot::getCookie()
{
    const qint64 msecSinceEpoch {
        QDateTime::currentMSecsSinceEpoch()
    };

    QUrl url(QUrl::fromUserInput(
                 QString(
                     "%1/%2?%3"
                     )
                 .arg(QString(m_cookieBaseUrl.toEncoded()))
                 .arg(m_pollnumber)
                 .arg(msecSinceEpoch)
                 )
             );

    QNetworkRequest request(
                url
                );
    request.setRawHeader("Host",
                         "polldaddy.com"
                         );
    request.setRawHeader("Referer",
                         m_voteBaseUrl.toEncoded()
                         );
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/59.0.3071.115 Safari/537.36"
                         );

    qDebug() << "[REQUESTING COOKIE] " << url.toEncoded() << endl;

    QNetworkReply* reply {
        m_net->get(request)
    };

    Q_ASSERT(reply);
    if (reply == nullptr) return;

    QObject::connect(
                reply,
                &QNetworkReply::finished,
                this,
                &Bot::onReplyGetCookie);
}

void Bot::vote()
{
    QUrlQuery query;
    query.addQueryItem("p",
                       m_pollnumber
                       );

    query.addQueryItem("b",
                       "0"
                       );

    query.addQueryItem("a",
                       m_ourpick + ","
                       );

    query.addQueryItem("o",
                       ""
                       );

    query.addQueryItem("va",
                       "16"
                       );

    query.addQueryItem("cookie",
                       "0"
                       );

    query.addQueryItem("n",
                       m_cookie
                       );

    query.addQueryItem("url",
                       m_voteBaseUrl.toEncoded()
                       );

    QUrl url("http://polls.polldaddy.com/vote-js.php");
    url.setQuery(query);

    QNetworkRequest request(
                url
                );

    qDebug() << "[VOTING] " << url.toEncoded() << endl;

    request.setRawHeader("Referer",
                         m_voteBaseUrl.toEncoded()
                         );
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/59.0.3071.115 Safari/537.36"
                         );

    QNetworkReply* reply {
        m_net->get(request)
    };

    Q_ASSERT(reply);
    if (reply == nullptr) return;

    QObject::connect(
                reply,
                &QNetworkReply::finished,
                this,
                &Bot::onReplyVote);
}

void Bot::onReplyGetCookie()
{
    QNetworkReply* reply {
        qobject_cast<QNetworkReply*>(sender())
    };
    if (reply == nullptr) return;

    QString content(reply->readAll());
    reply->deleteLater();

    QRegExp rx("'(.*)'");

    QString lastCookie(m_cookie);

    int index { rx.indexIn(content) };
    if (index >= 0) {
        m_cookie = rx.cap(1);
    }

    qDebug() << "[COOKIE] " << m_cookie << endl;

    if (m_cookie == lastCookie) {
        qDebug() << "Cookie has not changed ... delaying";
        QTimer::singleShot(
                    1000,
                    this,
                    &Bot::getCookie
                    );
    } else{
        qDebug() << "New cookie. Voting now.";
        vote();
    }
}

void Bot::onReplyVote()
{
    QNetworkReply* reply {
        qobject_cast<QNetworkReply*>(sender())
    };
    if (reply == nullptr) return;

    QByteArray content(reply->readAll());
    qDebug() << "[REPLY] " << content << endl;
    reply->deleteLater();

    // quick & dirty way of testing for result. actually parsing the json would be correct.
    if (content.contains(R"("result")")) {
        if (content.contains(R"("registered")")) {
            qDebug() << "[VOTE ACCEPTED] Count:" << ++(*m_voteCounter) << endl;

            if (--m_todo >= 0) {
                QTimer::singleShot(
                            1000,
                            this,
                            &Bot::getCookie
                            );
            } else {
                qDebug() << "[VOTE BURST DONE] Going to sleep for 2 minutes ..." << endl;

                m_todo = BurstSize;

                QTimer::singleShot(
                            120000,
                            this,
                            &Bot::getCookie
                            );
            }
        } else if (content.contains(R"("already-registered")")) {
            qDebug() << "[VOTE REJECTED] Going to sleep for 2 minutes ..." << endl;

            QTimer::singleShot(
                        120000,
                        this,
                        &Bot::getCookie
                        );
        } else {
            qDebug() << "[VOTE INCONCLUSIVE] Trying again ..." << endl;

            QTimer::singleShot(
                        1000,
                        this,
                        &Bot::getCookie
                        );
        }
    } else {
        qDebug() << "[SHADOWBANNED] Going to sleep for 2 minutes ..." << endl;

        QTimer::singleShot(
                    120000,
                    this,
                    &Bot::getCookie
                    );
    }
}

}
