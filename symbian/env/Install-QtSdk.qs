function Controller()
{
    installer.autoAcceptMessageBoxes();
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
}

Controller.prototype.IntroductionPageCallback = function()
{
    console.log("Qt SDK installer: introduction");
    gui.clickButton(buttons.NextButton, 1000);
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    console.log("Qt SDK installer: target directory C:\\QtSDK");
    var page = gui.currentPageWidget();
    page.TargetDirectoryLineEdit.setText("C:\\QtSDK");
    gui.clickButton(buttons.NextButton, 1000);
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    console.log("Qt SDK installer: keeping defaults except the unrelated Maemo/Madde toolchain");
    var page = gui.currentPageWidget();
    var components = installer.components();
    for (var i = 0; i < components.length; ++i) {
        var identity = components[i].name + " " + components[i].displayName;
        if (/madde|maemo|harmattan|meego/i.test(identity)) {
            console.log("Qt SDK installer: deselecting " + identity);
            page.deselectComponent(components[i].name);
        }
    }
    gui.clickButton(buttons.NextButton, 1000);
}

Controller.prototype.LicenseAgreementPageCallback = function()
{
    console.log("Qt SDK installer: accepting bundled component licenses");
    var page = gui.currentPageWidget();
    if (page.AcceptLicenseRadioButton)
        page.AcceptLicenseRadioButton.setChecked(true);
    if (page.AcceptLicenseCheckBox)
        page.AcceptLicenseCheckBox.setChecked(true);
    gui.clickButton(buttons.NextButton, 1000);
}

Controller.prototype.StartMenuDirectoryPageCallback = function()
{
    gui.clickButton(buttons.NextButton, 500);
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    console.log("Qt SDK installer: starting installation");
    gui.clickButton(buttons.NextButton, 1000);
}

Controller.prototype.FinishedPageCallback = function()
{
    console.log("Qt SDK installer: installation finished");
    var page = gui.currentPageWidget();
    if (page.RunItCheckBox)
        page.RunItCheckBox.setChecked(false);
    if (page.LaunchQtCreatorCheckBoxForm &&
        page.LaunchQtCreatorCheckBoxForm.launchQtCreatorCheckBox)
        page.LaunchQtCreatorCheckBoxForm.launchQtCreatorCheckBox.setChecked(false);
    gui.clickButton(buttons.FinishButton, 1000);
}
