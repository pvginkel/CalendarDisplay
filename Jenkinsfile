library('JenkinsPipelineUtils') _

withCredentials([
    string(credentialsId: 'WIFI_PASSWORD', variable: 'WIFI_PASSWORD'),
]) {
    podTemplate(inheritFrom: 'jenkins-agent-large', containers: [
        containerTemplate(name: 'idf', image: 'espressif/idf:v5.1.3', command: 'sleep', args: 'infinity', envVars: [
            containerEnvVar(key: 'WIFI_PASSWORD', value: '$WIFI_PASSWORD'),
        ])
    ]) {
        node(POD_LABEL) {
            stage('Build calendar display') {
                dir('CalendarDisplay') {
                    git branch: 'main',
                        credentialsId: '5f6fbd66-b41c-405f-b107-85ba6fd97f10',
                        url: 'https://github.com/pvginkel/CalendarDisplay.git'
                        
                    container('idf') {
                        // Necessary because the IDF container doesn't have support
                        // for setting the uid/gid.
                        sh 'git config --global --add safe.directory \'*\''
                        
                        sh '/opt/esp/entrypoint.sh scripts/dockerbuild.sh'
                    }
                }
            }
            
            stage('Deploy calendar display') {
                dir('HelmCharts') {
                    git branch: 'main',
                        credentialsId: '5f6fbd66-b41c-405f-b107-85ba6fd97f10',
                        url: 'https://github.com/pvginkel/HelmCharts.git'
                }

                dir('CalendarDisplay') {
                    sh 'cp build/esp32-calendar-display.bin calendar-display-ota.bin'

                    sh 'scripts/upload.sh ../HelmCharts/assets/kubernetes-signing-key calendar-display-ota.bin'
                }
            }
        }
    }
}
