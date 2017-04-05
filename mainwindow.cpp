#include "mainwindow.h"
#include "ui_mainwindow.h"

QSettings setting("3PR3", "PodcastFeed");

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

    //Create a folder for episodes already listened to.
    if(!QDir().exists(txtListened)){
        QDir().mkdir(txtListened);
    }

    if(!QDir().exists(xmlFolder)){
        QDir().mkdir(xmlFolder);
    }

    if(!QDir().exists(iconFolder)){
        QDir().mkdir(iconFolder);
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

    //Loading Settings
    LoadSettings();

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
    //Save Settings
    SaveSettings();

    canClose = false;
    this->show();
}

void MainWindow::closeWindow()
{
    canClose = true;
    this->close();
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
        ui->statusBar->showMessage(podcastMgr.addPodcast(itunesLink, "itunes"), 3000);
        updateUIPodcastList();
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
        ui->statusBar->showMessage(podcastMgr.addPodcast(rssLink, "rss"), 3000);
        updateUIPodcastList();
    }
}

void MainWindow::on_actionRefresh_Feed_triggered()
{
    ui->statusBar->showMessage("Refreshing Feed...");
    //re-download the xml and icon files
    refreshPodcasts();
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

        ui->statusBar->showMessage(podcastMgr.removePodcast(podcastName), 3000);
        //re-populate widgets
        updateUIPodcastList();
    }
}

bool MainWindow::FileExists(QString filechkPath){
    //Use QFileInfo for this filepath
    QFileInfo filechk(filechkPath);

    if(filechk.exists() && filechk.isFile()){
        return true;
    }
    else{
        return false;
    }
}

void MainWindow::on_PodcastList_clicked(const QModelIndex &index)
{
    QString podcastName = ui->PodcastList->item(index.row())->text();
    QString podcastDescription;
    QString podcastCategory;
    QString podcastAuthors;
    QStringList Episodes;
    QString podcastExplicit;
    QStringList ListenedEpisodes;

    QString podcastFilePath = xmlFolder + "/" + podcastName + ".xml";

    QFile xmlFile(podcastFilePath);
    if (!xmlFile.open(QFile::ReadOnly | QFile::Text)) {
        ui->statusBar->showMessage("Cannot load xml file...", 3000);
    }

    QXmlStreamReader xml(&xmlFile);

    bool firstSkipped = false;
    bool DescReached = false;
    bool explicitReached = false;
    bool categoryReached = false;
    bool authorReached = false;

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
        if(xml.qualifiedName() == "itunes:explicit" && !explicitReached){
            podcastExplicit = xml.readElementText();
            explicitReached = true;
        }
        if(xml.qualifiedName() == "itunes:category" && !categoryReached){
            podcastCategory = xml.attributes().value("text").toString();
            categoryReached = true;
        }
        if(xml.qualifiedName() == "itunes:author" && !authorReached){
            podcastAuthors = xml.readElementText();
            authorReached = true;
        }
        if(xml.name() == "description" && !DescReached){
                podcastDescription += "<h3>Podcast Description:</h3>";
                podcastDescription += xml.readElementText();
                //Check boolean if podcast description has already been taken
                DescReached = true;
        }
        if(token == QXmlStreamReader::StartElement) {
            if(xml.name() == "title" && xml.prefix().isEmpty() && firstSkipped) {
                Episodes << xml.readElementText();
            }
            if(xml.name() == "item" && !firstSkipped){
                firstSkipped = true;
            }
        }
     }

    xml.clear();
    xmlFile.close();

    //Merge Data for podcast description
    QString extraInfo = "<img src=\"" + iconFolder + "/"  + podcastName + ".bmp\""
                        + " alt=\"Podcast Icon\"" + " width=\"100\"" + " height=\"100\">";
    extraInfo += "<h3>Category: " + podcastCategory + "</h3>";
    extraInfo += "<h3>Author(s): " + podcastAuthors + "</h3>";
    extraInfo += "<h3> Podcast Explicit: " + podcastExplicit + "</h3>";
    podcastDescription.insert(0, extraInfo);


    ui->EpisodeList->clear();


    setting.beginGroup("MainWindow");
        if(setting.value("SortOrderAscend") == false){
            std::reverse(Episodes.begin(), Episodes.end());
            std::reverse(ListenedEpisodes.begin(), ListenedEpisodes.end());
        }
    setting.endGroup();

    ui->EpisodeList->addItems(Episodes);

    //Takes all the Episodes and checks whether there is already an existing filepath for them.
    //If the path exists, the episode has already been played
    foreach(QString epName, Episodes){
        //Create a filepath to be checked
        QString filechkPath = txtListened + "/" + podcastName + "/" + epName + ".txt";
        if(FileExists(filechkPath)){
            ListenedEpisodes << epName;
        }
    }

    //Declareing rowList for both foreach loops
    QList<QListWidgetItem*> rowList;

    //A QList is created of all the episodeNames that match.
    //The QList is a list of QListWidgetItems that need their color changed
    //Since they have been played.
    foreach(QString epName, ListenedEpisodes){
        rowList += ui->EpisodeList->findItems(epName, Qt::MatchContains);
    }

    //For each rowItem in rowList, the row is found and the text color is changed to grey
    //indicating that it has been previously played.
    foreach(QListWidgetItem* rowItem, rowList){
        rowItem->setTextColor(QColor("grey"));
        int row = ui->EpisodeList->row(rowItem);
        ui->EpisodeList->item(row)->setTextColor(QColor("grey"));
    }

    ui->Description->clear();
    ui->Description->setHtml(podcastDescription);
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

void MainWindow::refreshPodcasts(){
    //Open json file as read only
    QFile jsonFile(appDataFile);

    //if file does not exit return false
    if(jsonFile.open(QFile::ReadOnly)){
        //convert all object in the app data file into an array
        QJsonDocument document = QJsonDocument().fromJson(jsonFile.readAll());
        QJsonObject jsonObject = document.object();
        QJsonArray jsonArray = jsonObject["Podcasts"].toArray();

        //go through array and look for the podcast
        foreach (const QJsonValue & value, jsonArray) {
            QJsonObject obj = value.toObject();

            ui->statusBar->showMessage(podcastMgr.addPodcast(obj["rssLink"].toString(), "refresh"));
            }
        ui->statusBar->showMessage("Done Refreshing Feed!", 3000);
    } else {
        ui->statusBar->showMessage("Unable to Refresh Feed: Cannot open appData file.", 3000);
    }
    jsonFile.close();
}

//This function is used to create a new text file with the appropriate directory and episodename of
//episodes that have already been successfully played.
void MainWindow::CreateEpisodeTextFile(QString podcastName, QString episodeName){
    //Episode text file path
    QString episodetxtFile = txtListened + "/" + podcastName + "/" + episodeName + ".txt";
    QString episodetxtPathCreate = txtListened + "/" + podcastName + "/";

    if(!QDir().exists(episodetxtPathCreate)){
        QDir().mkdir(episodetxtPathCreate);
    }

    QFile txtfile(episodetxtFile);
    //Open file and write data
    if (txtfile.open(QFile::WriteOnly)){
        //Put duration in here eventually
        txtfile.write("Duration");
    }
    else{
        ui->statusBar->showMessage("Error creating text file for played song", 3000);
    }

    //close file
    txtfile.close();
}

void MainWindow::on_playPodcast_clicked()
{
    //Declarations
    QString podcastName;
    QString episodeName;
    int episodeRow;
    if(ui->PodcastList->selectedItems().size() != 0){
        podcastName = ui->PodcastList->currentItem()->text();
    }
    if(ui->EpisodeList->selectedItems().size() != 0){
        episodeName = ui->EpisodeList->currentItem()->text();
        episodeRow = ui->EpisodeList->currentRow();
    }
    
    //set message color to red
    ui->statusBar->setStyleSheet("color: red");

    setting.beginGroup("MainWindow");
            //Sets the boolean to check if buffering is enabled in the UI.
            bool bufferEnabled = setting.value("BufferingEnabled").toBool();
    setting.endGroup();


    // User selected an episode AND the player is not currently playing any audio
    if(!podcastName.isEmpty() && !episodeName.isEmpty()){
       if(player->state() == QMediaPlayer::PlayingState){
           player->stop();
           ui->pauseResumeAudio->setText("Pause");
           ui->pauseResumeAudio->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
       }
       //Creates the appropriate text file and path for the episode.
       CreateEpisodeTextFile(podcastName, episodeName);

       ui->statusBar->showMessage("Buffering Content, Please Wait...");
       ui->currentlyPlaying->setText(podcastName + ": "
                                     + episodeName);

       if(bufferEnabled){
           bufferPlayEpisode();
       }
       else{
           player->setMedia(episodeFile());
           player->play();
       }
    ui->statusBar->showMessage("Done Buffering!", 3000);
    //reset color to default
    ui->statusBar->setStyleSheet(styleSheet());

    //Changes the color of the current episodeRow in the EpisodeList to grey.
    ui->EpisodeList->item(episodeRow)->setTextColor(QColor("grey"));
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
    if(player->state() == QMediaPlayer::PlayingState){
        player->setPosition(player->position() - (15*1000));
    }
}

void MainWindow::on_skip_forward_clicked()
{
    //Vamsi: Skip ahead by 15 seconds
    if(player->state() == QMediaPlayer::PlayingState){
        player->setPosition(player->position() + (15*1000));
    }
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

    //Create episode text file
    //CreateEpisodeTextFile(podcastName, episodeName);

    return audioFile;
}
//end Author:Vamsidhar Allampati

void MainWindow::SaveSettings(){
    //Function used for default saving of settings.
    //Settings are also set in other function when applicable.
    setting.beginGroup("MainWindow");
        //Sets the bool settings for if AscendingSortOrder is enabled or disabled
        if(ui->actionSort_Order_Ascending->isEnabled() == true){
            setting.setValue("SortOrderAscend", false);
        }
        else if(ui->actionSort_Order_Ascending->isEnabled() == false){
            setting.setValue("SortOrderAscend", true);
        }
        else{
            //This is the default if there is no setting.
            setting.setValue("SortOrderAscend", true);
        }

        //Sets the bool setting for if Buffering is enabled or disabled
        if(ui->actionEnable_Buffering->isChecked()){
            setting.setValue("BufferingEnabled", true);
        }
        else{
            setting.setValue("BufferingEnabled", false);
        }

        //Sets the volume settings for later
        //setting.setValue("volume", player->volume());

        //Sets the Media Position of the last playing file.
        //setting.setValue("MediaPosition", player->position());
    setting.endGroup();
}

void MainWindow::LoadSettings(){
    //Load
        setting.beginGroup("MainWindow");

                //Determines if Buffering is already enabled or disabled in settings and
                //adjust the UI accordingly.
                //If there is no setting for Buffering then the default of BufferingEnabled = true
                //is enabled both in the settings and in the UI so they co-ordinate.

                if(setting.value("BufferingEnabled").toBool() == true){
                    ui->actionEnable_Buffering->setChecked(true);
                }
                else if(setting.value("BufferingEnabled").toBool() == false){
                    ui->actionEnable_Buffering->setChecked(false);
                }
                else{
                    //Default buffering when there is no previous setting
                    ui->actionEnable_Buffering->setChecked(false);
                    setting.setValue("BufferingEnabled", false);
                }



                //Determines if SortingOrder is Ascending or Descending;
                //SortOrderAscend = true means Sort Order is set to Ascending
                //SortOrderAscend = fasle means Sort Order is set to Descending
                if(setting.value("SortOrderAscend").toBool() == true){
                    //If the sort order is currently true for Ascending then you don't need set Ascending anymore.
                    ui->actionSort_Order_Ascending->setEnabled(false);
                    //If the sort order is currently true for Ascending then it means the option of Descending should be available.
                    ui->actionSort_Order_Descending->setEnabled(true);
                }
                else if(setting.value("SortOrderAscend").toBool() == false){
                    //If the sort order is currently false for Ascending then it means the option of Ascending should be available.
                    ui->actionSort_Order_Ascending->setEnabled(true);
                    //If the sort order is currentl false for Ascending then it means the options of Descending should not be available.
                    ui->actionSort_Order_Descending->setEnabled(false);
                }
                else{
                    //If the sort order is currently false for Ascending then it means the option of Ascending should be available.
                    ui->actionSort_Order_Ascending->setEnabled(true);
                    //If the sort order is currently false for Ascending then it means the option of Descending should not be available.
                    ui->actionSort_Order_Descending->setEnabled(false);
                    setting.setValue("SortOrderAscend", false);
                }

                //Sets the bool setting for if Buffering is enabled or disabled
                setting.setValue("BufferingEnabled", ui->actionEnable_Buffering->isChecked());
        setting.endGroup();
}

void MainWindow::on_actionEnable_Buffering_triggered()
{
    setting.beginGroup("MainWindow");
        //Sets the bool setting for if Buffering is enabled or disabled
        setting.setValue("BufferingEnabled", ui->actionEnable_Buffering->isChecked());
    setting.endGroup();
}

void MainWindow::on_actionSort_Order_Ascending_triggered()
{
    setting.beginGroup("MainWindow");
    setting.setValue("SortOrderAscend", true);
    ui->actionSort_Order_Descending->setEnabled(true);
    ui->actionSort_Order_Ascending->setEnabled(false);
    setting.endGroup();

    //Populate the widgets
    updateUIPodcastList();
}

void MainWindow::on_actionSort_Order_Descending_triggered()
{
    setting.beginGroup("MainWindow");
    setting.setValue("SortOrderAscend", false);
    ui->actionSort_Order_Descending->setEnabled(false);
    ui->actionSort_Order_Ascending->setEnabled(true);
    setting.endGroup();

    //Populate the widgets
    updateUIPodcastList();
}

void MainWindow::on_actionQuit_triggered()
{
    //Save Settings
    SaveSettings();

    closeWindow();
}

void MainWindow::on_actionClear_History_triggered()
{
    QDir history(txtListened);
    if(history.removeRecursively()){
        //recreatie the Folder
        if(!QDir().exists(txtListened)){
            QDir().mkdir(txtListened);
        }
        updateUIPodcastList();
        ui->statusBar->showMessage("History Cleared!");
    } else {
        updateUIPodcastList();
        ui->statusBar->showMessage("Unable to clear History...");
    }
}
