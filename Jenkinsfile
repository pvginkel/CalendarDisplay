// Builds the CalendarDisplay firmware and uploads it over the air through IoTSupport, at
// https://iot.ginbov.nl.
//
// Controller config:
//   - Job: Firmware/CalendarDisplay
//   - SCM: pvginkel/CalendarDisplay, branch main
//   - Script Path: Jenkinsfile

library identifier: 'JenkinsPipelineUtils', changelog: false

pipeline {
    agent {
        kubernetes {
            inheritFrom 'jenkins-agent-large'
            yamlMergeStrategy merge()
            yaml podYaml(images: [[image: 'espressif/idf:v5.5.3', name: 'idf']])
        }
    }

    options {
        // Without abortPrevious: an upload cut off by an abort leaves the devices on two firmware
        // versions.
        disableConcurrentBuilds()
        skipDefaultCheckout()
        timeout(time: 60, unit: 'MINUTES')
        timestamps()
    }

    triggers {
        githubPush()
    }

    stages {
        stage('Checkout') {
            steps {
                dir('CalendarDisplay') {
                    checkout scm
                }
                dir('esp-libs') {
                    git url: 'https://github.com/pvginkel/esp-libs.git', branch: 'main',
                        credentialsId: '5f6fbd66-b41c-405f-b107-85ba6fd97f10'
                }
            }
        }

        stage('Build firmware') {
            steps {
                script {
                    espFirmware.build(dir: 'CalendarDisplay')
                }
            }
        }

        stage('Deploy firmware') {
            steps {
                script {
                    espFirmware.upload(dir: 'CalendarDisplay')
                }
            }
        }
    }

    post {
        aborted {
            script {
                notify.error("${env.JOB_NAME} #${env.BUILD_NUMBER} aborted (timeout or hand)")
            }
        }
    }
}
