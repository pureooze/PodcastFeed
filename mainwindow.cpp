#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //Disable the resize grip on the lower right corner
    ui->statusBar->setSizeGripEnabled(false);


    //Check if Appdata folders exit, and create them if not
    if(!QDir().exists(appDataFolder)){
        QDir().mkdir(appDataFolder);
    }

    if(!QDir().exists(xmlFolder)){
        QDir().mkdir(xmlFolder);
    }

    if(!QDir().exists(iconFolder)){
        QDir().mkdir(iconFolder);
    }

    if(!QDir().exists(downloadFolder)){
        QDir().mkdir(downloadFolder);
    }

    //Populate the widgets
    updateUIPodcastList();

    //Settings
    ui->PodcastList->setIconSize(QSize(20,20));
    ui->Description->setOpenExternalLinks(true);
    //Connect Volume Slider to player volume
    ui->volumeSlider->setValue(10);
    player->setVolume(ui->volumeSlider->value());
    connect(ui->volumeSlider, SIGNAL(valueChanged(int)), player, SLOT(setVolume(int)));

    // Only show minimize button
    //setWindowFlags(Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    // Podcast List right click menu
    ui->PodcastList->setContextMenuPolicy(Qt::CustomContextMenu);
    //QMenu *podcastMenu = new QMenu(this);
    //podcastMenu.addAction(new QAction(tr("Download Episode"), this));

    connect(ui->PodcastList,SIGNAL(customContextMenuRequested(const QPoint &)),SLOT(podcastListMenu(const QPoint &)));

    // Episode List right click menu
    ui->EpisodeList->setContextMenuPolicy(Qt::CustomContextMenu);
    QAction *episodeDownloadAction = new QAction(tr("Download All Episodes"));
    episodeMenu->addAction(episodeDownloadAction);

    connect(episodeDownloadAction, SIGNAL(triggered(bool)), this, SLOT(downloadPodcast()));
    connect(ui->EpisodeList,SIGNAL(customContextMenuRequested(const QPoint &)),SLOT(episodeListMenu(const QPoint &)));

    // Tray icon menu initialization
    QAction *closeAction = new QAction("&Close", this);
    QMenu *menu = new QMenu(this);
    menu->addAction(closeAction);

    // Tray icon initialization
    QSystemTrayIcon *tray = new QSystemTrayIcon();
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(displayWindow()));
    tray->setIcon(QIcon(":/trayIcon.png"));
    tray->setToolTip("PodcastFeed");
    tray->setContextMenu(menu);
    tray->show();

    // Tray Icon Action Connects
    connect(closeAction, SIGNAL(triggered()), this, SLOT(closeWindow()));


    //Set Media Icons
    ui->playPodcast->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->pauseResumeAudio->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->stopAudio->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    ui->skip_forward->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    ui->skip_backward->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));

    //Set Media Connects
    connect(player, SIGNAL(positionChanged(qint64)), this, SLOT(updatePosition(qint64)));
    connect(player, SIGNAL(durationChanged(qint64)), this, SLOT(setSliderRange(qint64)));
    connect(ui->playerSlider, SIGNAL(valueChanged(int)), this, SLOT(setPosition(int)));

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    if(this->isMinimized() || canClose == true){
        event->accept();
    }else{
        event->ignore();
        canClose = true;
        this->hide();
    }
}

void MainWindow::displayWindow()
{
    canClose = false;
    this->show();
}

void MainWindow::closeWindow()
{
    canClose = true;
    this->close();
}

void MainWindow::episodeListMenu(const QPoint &pos)
{
    QPoint globalPos = ui->EpisodeList->mapToGlobal(pos);
    episodeMenu->exec(globalPos);
}

void MainWindow::podcastListMenu(const QPoint &pos)
{
    QPoint globalPos = ui->PodcastList->mapToGlobal(pos);
    //contextMenu.exec(globalPos);
}

void MainWindow::downloadPodcast()
{
    qDebug() << episodeFile();
    //When the http request is finished, call the storePodcast function
    QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(storePodcast(QNetworkReply*)));
    //http request
    manager->get(QNetworkRequest(episodeFile()));
}

void MainWindow::storePodcast(QNetworkReply *reply){
    //If there is a reply error then display message
    if (reply->error() == QNetworkReply::NoError) {

        //parse the json reply and get the rss link string
        qDebug() << ((QString)reply->readAll()).toUtf8();

        reply->deleteLater();
    }
    else {
        //reply error, display message
        ui->statusBar->showMessage("Network Request Failed..." + reply->errorString(), 3000);
        reply->deleteLater();
    }
}

void MainWindow::on_actionUsing_Itunes_Link_triggered()
{
    //Bool to tell if user clicked ok on dialog.
    bool ok;
    //call the Podcast dialog function and pass the label, and window title
    QString itunesLink = addPodcast_dlg("Itunes Link:", "Add Podcast using Itunes Link", ok);
    //Check if user clicked ok and it the string is empty
    if(ok && !itunesLink.isEmpty()){
        //Pass Itunes Link to getRSSurl
        //function will then get the rss url and call addPodcast Function
        //If there is an error, it is displayed on the status bar
        getRSSurl(itunesLink);
    }
}

void MainWindow::on_actionUsing_RSS_Link_triggered()
{
    //Bool to tell if user clicked ok on dialog.
    bool ok;
    //call the Podcast dialog function and pass the label, and window title
    QString rssLink = addPodcast_dlg("RSS Link:", "Add Podcast using RSS Link", ok);
    //Check if user clicked ok and it the string is empty
    if(ok && !rssLink.isEmpty()){
        //Call addPodcast function and pass rss link
        addPodcast(rssLink);
    }
}

void MainWindow::on_actionRefresh_Feed_triggered()
{
    //re-download xml files

    //re-download icons

    //re-populate widgets
    updateUIPodcastList();
}

void MainWindow::on_actionRemove_Podcast_triggered()
{
    bool ok;

    QStringList podcastList;
    //use the app data file to populate the podcast list
    QFile jsonFile(appDataFile);
    if(jsonFile.open(QFile::ReadOnly)){
        QJsonDocument document = QJsonDocument().fromJson(jsonFile.readAll());
        QJsonObject jsonObject = document.object();
        QJsonArray jsonArray = jsonObject["Podcasts"].toArray();

        foreach (const QJsonValue & value, jsonArray) {
            QJsonObject obj = value.toObject();

            podcastList << obj["podcastName"].toString();
        }

        jsonFile.close();
    }
    //Dialog asking the user what podcast they would like ot remove
    //Using a dropdown menu populated with podcast list
    QString podcastName = QInputDialog::getItem(this, "Remove Podcast",
                                         "Select Podcast to remove",
                                         podcastList, 0, false, &ok);
    if (ok && !podcastName.isEmpty()){
        //Remove podcast from app data file
        removePodcast_fromAppDataFile(podcastName);
        //Remove xml file
        QFile podcastXML(xmlFolder + "/" + podcastName + ".xml");
        podcastXML.remove();
        //Remove icon file
        QFile podcastIcon(iconFolder + "/" + podcastName + ".bmp");
        podcastIcon.remove();
        //re-populate widgets
        updateUIPodcastList();
    }
}

void MainWindow::on_PodcastList_clicked(const QModelIndex &index)
{
    QString podcastName = ui->PodcastList->item(index.row())->text();
    QString podcastDescription;
    QStringList Episodes;



    QString podcastFilePath = xmlFolder + "/" + podcastName + ".xml";

    QFile xmlFile(podcastFilePath);
    if (!xmlFile.open(QFile::ReadOnly | QFile::Text)) {
        ui->statusBar->showMessage("Cannot load xml file...", 3000);
    }

    QXmlStreamReader xml(&xmlFile);

    bool firstSkipped = false;
    bool DescReached = false;

    while(!xml.atEnd() && !xml.hasError()) {
        // Read next element
        QXmlStreamReader::TokenType token = xml.readNext();
        //If token is just StartDocument - go to next
        if(token == QXmlStreamReader::StartDocument) {
                continue;
        }
        //Description for Podcast
        if(xml.name() == "rss"){
            xml.readNext();
            }
        if(xml.name() == "channel"){
            xml.readNext();
        }
        if(xml.name() == "description" && !DescReached){
                podcastDescription += "Podcast Description: \n---------------------------\n";
                podcastDescription += xml.readElementText();
                //Check boolean if podcast description has already been taken
                DescReached = true;
        }
        if(token == QXmlStreamReader::StartElement) {
            if(xml.name() == "title" && xml.prefix().isEmpty() && firstSkipped) {
                Episodes << xml.readElementText();
            }
            if(xml.name() == "title" && !firstSkipped){
                firstSkipped = true;
            }
        }
     }

    xml.clear();
    xmlFile.close();

    ui->EpisodeList->clear();
    std::reverse(Episodes.begin(), Episodes.end());
    ui->EpisodeList->addItems(Episodes);
    ui->Description->clear();
    ui->Description->setText(podcastDescription);
}

void MainWindow::on_EpisodeList_clicked(const QModelIndex &index)
{
    QString podcastName = ui->PodcastList->currentItem()->text();
    QString episodeName = ui->EpisodeList->item(index.row())->text();
    QString episodeDescription;

    QString podcastFilePath = xmlFolder + "/" + podcastName + ".xml";

    QFile xmlFile(podcastFilePath);
    if (!xmlFile.open(QFile::ReadOnly | QFile::Text)) {
        ui->statusBar->showMessage("Cannot load xml file...", 3000);
    }

    QXmlStreamReader xml(&xmlFile);

    bool descriptionReached = false;

    while(!xml.atEnd() && !xml.hasError()) {
        // Read next element
        QXmlStreamReader::TokenType token = xml.readNext();
        //If token is just StartDocument - go to next
        if(token == QXmlStreamReader::StartDocument) {
                continue;
        }
        //If token is StartElement - read it
        if(token == QXmlStreamReader::StartElement) {
            if(xml.name() == "title" && xml.prefix().isEmpty() && xml.readElementText() == episodeName){
                descriptionReached = true;
            }

            if(xml.name() == "description" && descriptionReached) {
                episodeDescription = xml.readElementText();
                break;
            }
        }
    }
    xmlFile.close();

    episodeDescription.insert(0, "<b>Description:</b>");

    ui->Description->clear();
    ui->Description->setHtml(episodeDescription);
}

void MainWindow::updateUIPodcastList(){
    //clear the display widgets
    ui->PodcastList->clear();
    ui->EpisodeList->clear();
    ui->Description->clear();

    //Open the app data file
    QFile jsonFile(appDataFile);
    if(jsonFile.open(QFile::ReadOnly)){
        QJsonDocument document = QJsonDocument().fromJson(jsonFile.readAll());
        QJsonObject jsonObject = document.object();
        QJsonArray jsonArray = jsonObject["Podcasts"].toArray();

        //go through the podcast list
        foreach (const QJsonValue & value, jsonArray) {
            QJsonObject obj = value.toObject();
            //icon path for current podcast
            QString iconPath = iconFolder + "/" + obj["podcastName"].toString() + ".bmp";
            //add the podcast and its icon to the podcast list widget.
            new QListWidgetItem(QIcon(iconPath),obj["podcastName"].toString(), ui->PodcastList);
        }
        jsonFile.close();
    }
}

//Custom Dialog for Adding Podcast
QString MainWindow::addPodcast_dlg(QString Label, QString Title, bool &ok){
    //Create New Dialog
    QInputDialog *dlg = new QInputDialog();
    //Set the input as text input
    dlg->setInputMode(QInputDialog::TextInput);
    //Label for text input
    dlg->setLabelText(Label);
    //Label for Dialog
    dlg->setWindowTitle(Title);
    //Set Dialog Size
    dlg->resize(600,500);
    //Exec Dialog and return true is user clicks ok, else return false
    //Store this value in bool reference ok.
    ok = dlg->exec();
    //Return the Text From the Text Input
    return dlg->textValue();
}


//Take Itunes Link, make http request, get json reply
//Parse json reply and find the rss link for podcast
void MainWindow::getRSSurl(QString itunesLink){
    //If the link is an itunes link and for a podcast then continue
    if(itunesLink.contains("itunes.apple.com") && itunesLink.contains("podcast") && itunesLink.contains("id")){
        //Get the index of the id string
        int idPosition = itunesLink.lastIndexOf("id");
        idPosition += 2;
        //Isolate everythign after id
        itunesLink = itunesLink.remove(0, idPosition);
        //Remove everything that is not the id on the right of the string
        while(itunesLink.toInt() == 0 && !itunesLink.isEmpty()){
            itunesLink.chop(1);
        }

        QString PodcastID = itunesLink;

        //If unable to find a podcast ID, return false
        if (PodcastID.toInt() == 0 || PodcastID.isEmpty()){
            ui->statusBar->showMessage("Itunes Link Invalid!", 3000);
        }
        //Create the request url using the podcast id
        QUrl url("https://itunes.apple.com/lookup?id=" + PodcastID + "&entity=podcast");
        //create new network manager
        //manager = new QNetworkAccessManager();
        //When the http request is finished, call the parseItunesReply function
        QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(parseItunesReply(QNetworkReply*)));
        //http request
        manager->get(QNetworkRequest(url));

    } else {
        ui->statusBar->showMessage("Incorrect Itunes Link!", 3000);
    }
}

void MainWindow::parseItunesReply(QNetworkReply *reply){
    //If there is a reply error then display message
    if (reply->error() == QNetworkReply::NoError) {

        //parse the json reply and get the rss link string
        QString rssLink = QJsonDocument::fromJson(((QString)reply->readAll()).toUtf8())
                        .object()
                        .value("results")
                        .toArray()
                        .at(0)
                        .toObject().value("feedUrl").toString();

        //Call addPodcast function and pass rss link
        addPodcast(rssLink);

        reply->deleteLater();
    }
    else {
        //reply error, display message
        ui->statusBar->showMessage("Network Request Failed..." + reply->errorString(), 3000);
        reply->deleteLater();
    }
}

void MainWindow::addPodcast(QString rssLink){
    //Create a event loop to keep everythin inline, rather than using other functions.
    QEventLoop eventLoop;
    //create new network manager
    manager = new QNetworkAccessManager();
    //When the http request is finished, end the eventloop
    QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    //Make the http request and store as a QNetworkReply Object
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(rssLink)));
    //Start the loop
    eventLoop.exec();

    QString podcastName, podcastIconURL;
    bool nameReached = false, iconReached = false;

    //check if reply is not xml or if there is an error
    if (reply->error() == QNetworkReply::NoError && reply->header(QNetworkRequest::ContentTypeHeader).toString().contains("xml")){
        QByteArray rawReply = reply->readAll();
        //Create a xml parser object
        QXmlStreamReader podcastXml;
        //Pass the reply to the xml parser
        podcastXml.addData(rawReply);
        //Loop until end of xml doc
        while(!podcastXml.atEnd() && !podcastXml.hasError()) {
            // Read next element
            QXmlStreamReader::TokenType token = podcastXml.readNext();
            //If token is just StartDocument - go to next
            if(token == QXmlStreamReader::StartDocument) {
                    continue;
            }
            //If token is StartElement - read it
            if(token == QXmlStreamReader::StartElement){
                //If the element name is title and is the first title element, save it
                if(podcastXml.name() == "title" && !nameReached){
                    podcastName = podcastXml.readElementText();
                    nameReached = true;
                }
                //If the element name is image and is the first image element, save the url
                if(podcastXml.qualifiedName() == "itunes:image" && !iconReached){
                    podcastIconURL = podcastXml.attributes().value("href").toString();
                    iconReached = true;
                }
                //If both name and icon url have been parsed, then break loop
                if(nameReached && iconReached){
                    break;
                }
            }
        }

        if(!podcastName.isEmpty() && !podcastIconURL.isEmpty()){
            storeXmlFile(podcastName, rawReply);
            storeIcon(podcastName, podcastIconURL);
            addPodcast_toAppDataFile(podcastName, rssLink);
            //re-populate widgets
            updateUIPodcastList();
        }

    } else {
        //reply error or reply is not xml
        ui->statusBar->showMessage("RSS Link Invalid or Request Failed", 3000);
        reply->deleteLater();
    }
}

void MainWindow::storeXmlFile(QString podcastName, QByteArray rawReply){
    //Podcast xml file path
    QString podcastFile = xmlFolder + "/" + podcastName + ".xml";

    QFile xmlfile(podcastFile);
    //Open the file and write the data to it
    if (xmlfile.open(QFile::WriteOnly) )
    {
        xmlfile.write(rawReply);
    } else {
        ui->statusBar->showMessage("Error Saving xml file", 3000);
    }

    //close file
    xmlfile.close();
}

void MainWindow::storeIcon(QString podcastName, QString iconURL){
    //Create a event loop to keep everythin inline, rather than using other functions.
    QEventLoop eventLoop;
    //create new network manager
    manager = new QNetworkAccessManager();
    //When the http request is finished, end the eventloop
    QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    //Make the http request and store as a QNetworkReply Object
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(iconURL)));
    //Start the loop
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        //Icon File Path
        QString IconPath = iconFolder + "/" + podcastName + ".bmp";
        //using QImage to convert from unknown format to bmp
        QImage icon;
        //load reply data into icon
        icon.loadFromData(reply->readAll());
        //unless icon data is null store image as bmp at the given path
        if(icon.isNull()){
           statusBar()->showMessage("Cannot Store Icon", 3000);
        } else {
            icon.save(IconPath);
        }
        reply->deleteLater();
    } else {
        //Failed to get icon
        statusBar()->showMessage("Failed to store podcast icon", 3000);
        reply->deleteLater();
    }
}

void MainWindow::addPodcast_toAppDataFile(QString podcastName, QString rssLink){
    //check if the podcast already exists
    if(!checkPodcastExists(podcastName)){

        QFile jsonFile(appDataFile);
        jsonFile.open(QFile::ReadWrite);

        QJsonDocument document = QJsonDocument().fromJson(jsonFile.readAll());
        //If this is a new file, i.e first time adding a podcast, create the json object and store in file
        if(jsonFile.pos() == 0){
            QJsonObject jsonObject;
            QJsonObject arrayObject{
                {"podcastName", podcastName},
                {"rssLink", rssLink}
            };
            QJsonArray jsonArray;
            jsonArray.append(arrayObject);
            jsonObject.insert("Podcasts", jsonArray);
            document.setObject(jsonObject);
            jsonFile.write(document.toJson());
            jsonFile.close();
        }else{
        //If this is not a new file, edit the json object and store in file
            QJsonObject jsonObject = document.object();
            QJsonObject arrayObject{
                {"podcastName", podcastName},
                {"rssLink", rssLink}
            };
            QJsonArray jsonArray = jsonObject["Podcasts"].toArray();
            jsonArray.append(arrayObject);
            jsonObject.insert("Podcasts", jsonArray);
            document.setObject(jsonObject);
            jsonFile.close();
            //reopen file inorder to clear existing data and place the updated podcasts object
            jsonFile.open(QFile::WriteOnly | QFile::Truncate);
            jsonFile.write(document.toJson());
            jsonFile.close();

        }

        ui->statusBar->showMessage("Podcast Added", 3000);
    }else{
        statusBar()->showMessage("Podcast Already Exists!", 3000);
    }
}

void MainWindow::removePodcast_fromAppDataFile(QString podcastName){
    if(checkPodcastExists(podcastName)){
        //keep track of the array element in foreach loop
        int podcastNumber = 0;

        QFile jsonFile(appDataFile);
        jsonFile.open(QFile::ReadWrite);

        QJsonDocument document = QJsonDocument().fromJson(jsonFile.readAll());

        QJsonObject jsonObject = document.object();

        QJsonArray jsonArray = jsonObject["Podcasts"].toArray();
        foreach (const QJsonValue & value, jsonArray) {
            QJsonObject obj = value.toObject();
            if(obj["podcastName"].toString() == podcastName){
                //if the podcast is found, remove it from array using the podcastNumber
                jsonArray.removeAt(podcastNumber);
                break;
            }else{
                //else update number
                podcastNumber++;
            }
        }
        jsonObject.insert("Podcasts", jsonArray);
        document.setObject(jsonObject);
        jsonFile.close();
        //reopen file inorder to clear existing data and place the updated podcasts object
        jsonFile.open(QFile::WriteOnly | QFile::Truncate);
        jsonFile.write(document.toJson());
        jsonFile.close();

        ui->statusBar->showMessage("Podcast Removed", 3000);
    }else{
        ui->statusBar->showMessage("Podcast does not exist!", 3000);
    }
}

bool MainWindow::checkPodcastExists(QString podcastName){
    //Open json file as read only
    QFile jsonFile(appDataFile);
    //if file does not exit return false
    if(!jsonFile.open(QFile::ReadOnly)){
        return false;
    }else{
        //convert all object in the app data file into an array
        QJsonDocument document = QJsonDocument().fromJson(jsonFile.readAll());
        QJsonObject jsonObject = document.object();
        QJsonArray jsonArray = jsonObject["Podcasts"].toArray();

        //go through array and look for the podcast
        foreach (const QJsonValue & value, jsonArray) {
            QJsonObject obj = value.toObject();
            if(obj["podcastName"].toString() == podcastName){
                //if the podcast is found return true
                jsonFile.close();
                return true;
            }
        }
    }
    //If the podcast is not found return false
    jsonFile.close();
    return false;
}

void MainWindow::on_playPodcast_clicked()
{
    // Get the values selected by the user, this should work regardless of if the widget is model or item based
    QModelIndexList list = ui->EpisodeList->selectionModel()->selectedIndexes();

    // User selected an episode AND the player is not currently playing any audio
    if(list.length() > 0 && player->state() == QMediaPlayer::StoppedState){
        ui->statusBar->setStyleSheet("color: red");
        ui->statusBar->showMessage("Buffering Content, Please Wait...");
        bufferPlayEpisode();
        ui->statusBar->showMessage("Done Buffering!", 3000);
    }else if (list.length() > 0){
        player->stop();
        ui->statusBar->setStyleSheet("color: red");
        ui->statusBar->showMessage("Buffering Content, Please Wait...");
        bufferPlayEpisode();
        ui->statusBar->showMessage("Done Buffering!", 3000);
    }
}

void MainWindow::bufferPlayEpisode(){
    //Create a event loop to keep everythin inline, rather than using other functions.
    QEventLoop eventLoop;
    //create new network manager
    QNetworkAccessManager *manager = new QNetworkAccessManager();
    //When the http request is finished, end the eventloop
    QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));

    QNetworkRequest request;
    request.setUrl(episodeFile());
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);

    QNetworkReply *audioReply = manager->get(request);
    audioReply->setReadBufferSize(0);
    eventLoop.exec();

    immPlay.open(QBuffer::WriteOnly | QBuffer::Truncate);
    immPlay.write(audioReply->readAll());
    immPlay.close();

    immPlay.open(QIODevice::ReadOnly);
    player->setMedia(QMediaContent(), &immPlay);
    player->play();
}

void MainWindow::on_stopAudio_clicked()
{
    player->stop();
}

void MainWindow::on_pauseResumeAudio_clicked()
{
    //Vamsi: If audio is playing pause and change text to resume
    if(player->state() == QMediaPlayer::PlayingState){
        player->pause();
        ui->pauseResumeAudio->setText("Resume");
        ui->pauseResumeAudio->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    //Vamsi: If audio is pause then play audio and change text to pause
    }else if(player->state() == QMediaPlayer::PausedState){
        player->play();
        ui->pauseResumeAudio->setText("Pause");
        ui->pauseResumeAudio->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    }
}

void MainWindow::on_skip_backward_clicked()
{
    //Vamsi: Skip backward by 15 seconds
    player->setPosition(player->position() - (15*1000));
}

void MainWindow::on_skip_forward_clicked()
{
    //Vamsi: Skip ahead by 15 seconds
    player->setPosition(player->position() + (15*1000));
}

//Author:Vamsidhar Allampati
//Set the slider range after buffer is filled
void MainWindow::setSliderRange(qint64 duration){
    QTime episodeDuration(0,0,0,0);
    ui->playerSlider->setRange(0, duration);
    ui->playerSlider->setTickInterval(duration/30);
    ui->durationLabel->setText("/ " + episodeDuration.addMSecs(duration).toString());
}
//Set the postion as the audio plays
void MainWindow::updatePosition(qint64 timeElapsed){
    if(!ui->playerSlider->isSliderDown()){
        QTime elapsedTime(0,0,0,0);
        ui->playerSlider->setValue(timeElapsed);
        ui->elapsedLabel->setText(elapsedTime.addMSecs(timeElapsed).toString());
    }
}
//if the user drags the slider, audio position is updated
void MainWindow::setPosition(int position){
    // avoid seeking when the slider value change is triggered from updatePosition()
    if (qAbs(player->position() - position) > 10){
        player->setPosition(position);
    }
}

//get the file url by parsing xml file.
QUrl MainWindow::episodeFile()
{
    QString podcastName = ui->PodcastList->currentItem()->text();
    QString episodeName = ui->EpisodeList->currentItem()->text();
    QUrl audioFile;

    QString podcastFilePath = xmlFolder + "/" + podcastName + ".xml";

    QFile xmlFile(podcastFilePath);
    if (!xmlFile.open(QFile::ReadOnly | QFile::Text)) {
        ui->statusBar->showMessage("Cannot load xml file...", 3000);
    }

    QXmlStreamReader xml(&xmlFile);

    bool episodeReached = false;

    while(!xml.atEnd() && !xml.hasError()) {
        // Read next element
        QXmlStreamReader::TokenType token = xml.readNext();
        //If token is just StartDocument - go to next
        if(token == QXmlStreamReader::StartDocument) {
                continue;
        }
        //If token is StartElement - read it
        if(token == QXmlStreamReader::StartElement) {

            if(xml.name() == "title" && xml.prefix().isEmpty() && xml.readElementText() == episodeName){
                episodeReached = true;
            }

            if(xml.name() == "enclosure" && episodeReached) {
                audioFile = xml.attributes().value("url").toString();
                break;
            }
        }
    }
    xmlFile.close();

    return audioFile;
}
//end Author:Vamsidhar Allampati
